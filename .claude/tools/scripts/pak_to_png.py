"""Convert Helbreath PAK files (including the encrypted Korea 2024 ones) to PNG sheets.

Usage:
  python -I pak_to_png.py <pak_dir> <out_root> [--only NAME ...]
  python -I pak_to_png.py --selftest <raw.pak> <decrypted_reference.pak>

For each <pak_dir>/<name>.pak writes
  <out_root>/<name>/<index:03d>.png   full sprite sheet (RGBA, colour key -> alpha 0)
  <out_root>/<name>/frames.json       per sprite: colour key, frame rects and pivots
A pak that fails to parse is logged and skipped. ASCII output only.

Format (plain 1999 PAK, as read by sources/dependencies/client/includes/PAK.h):
  file header 20 bytes: "<Pak file header>" + 3 pad          (PAK.h:27-32)
  u32 sprite_count                                           (PAK.h:83)
  sprite_count x {u32 offset, u32 size}                      (PAK.h:41-44)
  sprite at offset: 100-byte header ("<Sprite File Header>" + 80 pad)  (PAK.h:34-39)
    u32 rect_count, rect_count x {u16 x,y,w,h; i16 pivotX,pivotY} (PAK.h:18-25)
    u32 colour key (the "padding" the client skips, PAK.h:139)
    image bytes = size - (100 + 4 + rc*12) - 4              (PAK.h:146)
  The Korea 2024 paks are encrypted; scheme per Original-Helbreath/History
  sprites/README.md (see decrypt_pak below). Image is BMP (or PNG).
"""
import io
import json
import os
import struct
import sys
import traceback

import numpy
from PIL import Image

KEY = bytes([0x4C, 0xB2, 0x3E, 0x1A])
FILE_HDR = b"<Pak file header>"
SPR_HDR = b"<Sprite File Header>"


def _xor_key(buf, start, end, base):
    """XOR buf[start:end] with the repeating key, key index restarting at base."""
    for i in range(start, end):
        buf[i] ^= KEY[(i - base) & 3]


def _unchain(buf, start, end):
    """Undo the running XOR e[i] = p[i] ^ e[i-4] (measured against the derived copies):
    p[i] = e[i] ^ e[i-4], so walk descending so e[i-4] is still encoded."""
    for i in range(end - 1, start + 3, -1):
        buf[i] ^= buf[i - 4]


def decrypt_pak(raw, chain_mode="plain"):
    """Return plain-format pak bytes from an encrypted Korea 2024 pak."""
    b = bytearray(raw)
    # bytes 0-19 are not encrypted, just not the plain signature: restore it
    b[0:20] = FILE_HDR + b"\x00\x00\x00"
    # table: key restarts at offset 20
    count_end = 24
    _xor_key(b, 20, count_end, 20)
    count = struct.unpack_from("<I", b, 20)[0]
    if count == 0 or count > 1500:
        raise ValueError("bad sprite count %d" % count)
    table_end = 24 + 8 * count
    _xor_key(b, 24, table_end, 20)
    entries = [struct.unpack_from("<II", b, 24 + 8 * i) for i in range(count)]
    for off, size in entries:
        if off + size > len(b) or size < 108:
            raise ValueError("sprite entry out of range")
        # key pass over the whole sprite, restarting at the sprite start
        _xor_key(b, off, off + size, off)
        # header region (100 bytes)
        _unchain(b, off, off + 100)
        rc = struct.unpack_from("<I", b, off + 100)[0]
        if rc == 0:
            continue  # empty sprite (PAK.h:190)
        if rc > 1000:
            raise ValueError("bad rect count %d" % rc)
        ft = off + 104
        fe = ft + rc * 12
        _unchain(b, ft, fe)
        img = fe + 4
        _unchain(b, img, off + size)
    return bytes(b)


def repair_bmp(img):
    """Restore the BMP header fields the encryption garbles (bytes 12-17, 'BM')."""
    d = bytearray(img)
    if len(d) < 54:
        return bytes(d)
    if d[0:2] != b"BM":
        d[0:2] = b"BM"
    d[12:14] = b"\x00\x00"
    d[14:18] = struct.pack("<I", 40)
    # README: last 2 bytes of 4-aligned bitmaps are 00 00 (inferred by that project)
    if len(d) % 4 == 0:
        d[-2:] = b"\x00\x00"
    return bytes(d)


def repad_24bpp(d):
    """Some 24bpp bitmaps (Telescope2[5]) store rows unpadded; pad rows to 4 bytes."""
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0]
    off = struct.unpack_from("<I", d, 10)[0]
    if bpp != 24:
        raise ValueError("cannot repair non-24bpp bitmap")
    h = abs(h)
    row = w * 3
    if len(d) - off != row * h:
        raise ValueError("size does not match unpadded rows")
    pad = b"\x00" * ((4 - row % 4) % 4)
    body = b"".join(d[off + y * row:off + (y + 1) * row] + pad for y in range(h))
    return bytes(d[:off]) + body


def parse_plain(data):
    if data[:17] != FILE_HDR:
        raise ValueError("bad file header")
    count = struct.unpack_from("<I", data, 20)[0]
    if count == 0 or count > 1500:
        raise ValueError("bad sprite count %d" % count)
    sprites = []
    for i in range(count):
        off, size = struct.unpack_from("<II", data, 24 + 8 * i)
        if off + size > len(data) or size < 108:
            raise ValueError("sprite %d entry out of range" % i)
        if data[off:off + 20] != SPR_HDR:
            raise ValueError("sprite %d bad header" % i)
        rc = struct.unpack_from("<I", data, off + 100)[0]
        if rc == 0:
            sprites.append({"rects": [], "key": 0, "image": b""})  # empty sprite
            continue
        if rc > 1000:
            raise ValueError("sprite %d bad rect count" % i)
        rects = [struct.unpack_from("<HHHHhh", data, off + 104 + 12 * k) for k in range(rc)]
        kp = off + 104 + 12 * rc
        key = struct.unpack_from("<I", data, kp)[0]
        img = data[kp + 4:off + size]
        sprites.append({"rects": rects, "key": key, "image": img})
    return sprites


def to_rgba(img_bytes, key):
    """Decode BMP/PNG bytes to an RGBA image; the colour key becomes alpha 0.

    Key rule (PAKLib README): nonzero key field is the key colour; otherwise the
    top-left pixel is the key, as the game client does.
    """
    if img_bytes[:8] == b"\x89PNG\r\n\x1a\n":
        return Image.open(io.BytesIO(img_bytes)).convert("RGBA"), "png"
    fixed = repair_bmp(img_bytes)
    try:
        im = Image.open(io.BytesIO(fixed))
        im.load()
    except Exception:
        fixed = repad_24bpp(fixed)
        im = Image.open(io.BytesIO(fixed))
        im.load()
    rgb = im.convert("RGB")
    arr = numpy.array(rgb.convert("RGBA"))

    def key_mask(c):
        return (arr[:, :, 0] == c[0]) & (arr[:, :, 1] == c[1]) & (arr[:, :, 2] == c[2])

    mask = None
    how = "topleft"
    if key:
        # Measured against the top-left pixel of every non-zero-key sprite: the u32
        # holds bytes 00 R G B in file order, i.e. R=(k>>8), G=(k>>16), B=(k>>24).
        kc = ((key >> 8) & 255, (key >> 16) & 255, (key >> 24) & 255)
        m = key_mask(kc)
        if m.any():
            mask, how = m, "field"
        else:
            how = "topleft_fallback"
    if mask is None:
        mask = key_mask(rgb.getpixel((0, 0)))
    arr[mask] = (0, 0, 0, 0)
    return Image.fromarray(arr, "RGBA"), how


def convert_pak(path, out_root, encrypted=True):
    name = os.path.splitext(os.path.basename(path))[0]
    raw = open(path, "rb").read()
    data = decrypt_pak(raw) if encrypted and raw[:17] != FILE_HDR else raw
    sprites = parse_plain(data)
    out_dir = os.path.join(out_root, name)
    os.makedirs(out_dir, exist_ok=True)
    meta = []
    for i, s in enumerate(sprites):
        if not s["rects"]:
            meta.append({"index": i, "empty": True, "frames": []})
            continue
        try:
            im, how = to_rgba(s["image"], s["key"])
            im.save(os.path.join(out_dir, "%03d.png" % i), optimize=False)
            meta.append({
                "index": i, "file": "%03d.png" % i, "width": im.size[0], "height": im.size[1],
                "color_key_field": s["key"], "color_key_source": how,
                "frames": [{"x": r[0], "y": r[1], "w": r[2], "h": r[3], "pivot_x": r[4], "pivot_y": r[5]}
                           for r in s["rects"]],
            })
        except Exception as e:
            meta.append({"index": i, "error": str(e).encode("ascii", "replace").decode()})
            print("  sprite %s[%d] failed: %s" % (name, i, str(e).encode("ascii", "replace").decode()))
    with open(os.path.join(out_dir, "frames.json"), "w", encoding="utf-8") as f:
        json.dump({"pak": name + ".pak", "sprite_count": len(sprites), "sprites": meta}, f, indent=1)
    return len(sprites), sum(1 for m in meta if "error" not in m)


def selftest(raw_path, ref_path):
    raw = open(raw_path, "rb").read()
    ref = open(ref_path, "rb").read()
    dec = decrypt_pak(raw)
    print("len raw/dec/ref", len(raw), len(dec), len(ref))
    diffs = [i for i in range(min(len(dec), len(ref))) if dec[i] != ref[i]]
    print("differing bytes:", len(diffs), "first:", diffs[:20])
    a, b = parse_plain(dec), parse_plain(ref)
    same_rects = all(x["rects"] == y["rects"] and x["key"] == y["key"] for x, y in zip(a, b))
    print("sprites", len(a), len(b), "rects/key equal:", same_rects)
    same_img = sum(1 for x, y in zip(a, b) if repair_bmp(x["image"]) == repair_bmp(y["image"]))
    print("images equal after header repair:", same_img, "of", len(a))


def main(argv):
    if argv and argv[0] == "--selftest":
        selftest(argv[1], argv[2])
        return 0
    pak_dir, out_root = argv[0], argv[1]
    only = set()
    if "--only" in argv:
        only = set(x.lower() for x in argv[argv.index("--only") + 1:])
    names = sorted(n for n in os.listdir(pak_dir) if n.lower().endswith(".pak"))
    ok = fail = pngs = 0
    failures = []
    for n, fn in enumerate(names, 1):
        if only and os.path.splitext(fn)[0].lower() not in only:
            continue
        try:
            total, good = convert_pak(os.path.join(pak_dir, fn), out_root)
            pngs += good
            if good == total:  # good counts empty sprites too
                ok += 1
            else:
                fail += 1
                failures.append("%s: %d of %d sprites failed" % (fn, total - good, total))
        except Exception as e:
            fail += 1
            msg = "%s: %s" % (fn, str(e).encode("ascii", "replace").decode())
            failures.append(msg)
            print("FAIL " + msg)
        if n % 25 == 0:
            print("progress %d/%d" % (n, len(names)))
            sys.stdout.flush()
    print("paks_ok=%d paks_with_failures=%d pngs=%d" % (ok, fail, pngs))
    for f in failures:
        print("  " + f)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
