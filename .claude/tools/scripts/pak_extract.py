"""Lossless Helbreath .pak extractor and round-trip verifier (HH-0052).

Every .pak is unpacked into a folder holding the embedded images exactly as stored
(byte for byte) plus manifest.json with every header field, table entry, sprite and
frame; anything the format does not explain is kept as a raw side file and labelled
"unknown" in the manifest. --verify rebuilds each .pak from its folder alone and
byte-compares it with the original. The tool never deletes, moves or edits a .pak.

Usage (run from anywhere; ASCII output only):
  python -I pak_extract.py --dry-run                 list paks, target folders, clashes, size estimate
  python -I pak_extract.py                           extract everything in place
  python -I pak_extract.py --verify                  rebuild from folders, byte-compare with the paks
  python -I pak_extract.py --only ogre helm          limit to pak basenames or repo paths
  python -I pak_extract.py --out DIR                 mirrored tree under DIR instead of in place; DIR must be
                                                     strictly inside <repo>/build, on the repo's drive and
                                                     not a \\\\?\\, device or UNC path
  python -I pak_extract.py --root DIR                source tree other than the repo (tests); DIR must be
                                                     strictly inside <repo>/build
  The repo root and its ancestors (any spelling, checked by file identity) and any path through a
  symlink or junction are always refused (exit 2). Needs Python 3.12+ (os.path.isjunction).

Output for staged/client/sprites/npcs/ogre.pak:
  in place (default)   staged/client/sprites/npcs/ogre/
  --out DIR            DIR/staged/client/sprites/npcs/ogre/
In place, targets must lie strictly inside <tree>/staged/ or <tree>/tools/display_id_manager/sprites/,
and those bases (and every folder on the way to them) must be plain, canonical folders.
After an in-place run with no failures, staged/client/sprites/index.json and
tools/display_id_manager/sprites/index.json are written, keyed by the path below sprites/ without
".pak" ("npcs/ogre"), each only when the scan was complete and every pak under that folder (not just
the --only ones) rebuilds byte-identical; otherwise that index is not written.

In-place safety: the extracted folders are the hand-edited source of truth, so in-place mode never
deletes, replaces or overwrites anything. An existing <stem>/ or <stem>.tmp/ refuses that pak,
whatever its manifest says; clear a leftover by hand. New output is written to <stem>.tmp/ and renamed.

Mirror (--out) safety: an existing output folder is cleared only when it is canonical (resolves to
itself), strictly inside the output root, has no symlink or junction in its ancestry, is not the repo
or an ancestor by file identity, holds a manifest.json with tool "pak_extract", this FORMAT_VERSION and
the exact source pak path, holds exactly the files that manifest lists (no .pak, link or extra entry)
and still rebuilds to its source_sha256 (so an edited image refuses). A clear first writes a
.pak_extract_clearing.json marker, then removes files, manifest and marker last; there is no rmtree.
A folder holding our marker (an interrupted clear) may hold any subset of its listed files. A leftover
<folder>.tmp is ours by its stub or by our manifest for that exact source.

Accepted risk: between the last check and each create, rename or delete there is a check-then-act
window. Exploiting it needs a concurrent process of the same user swapping in a link at that moment,
which is accepted for a developer tool.

Source discovery: every *.pak under the tree root except the top-level game/, bin/, build/, .git/,
.claude/ and working/ (game/bin/build hold output copies, .claude holds scratch). The walk never
follows a symlink or junction, never crosses a filesystem (st_dev) boundary and never enters a folder
holding a pak_extract manifest. Such a folder counts as extracted only when its manifest's source is
the sibling <folder>.pak and that pak exists; any other is a scan issue. Skipped links, crossings,
stray manifests and walk errors are reported and block the index.

Folder layout:
  manifest.json          everything below, plus source size and sha256
  NNN.bmp|png|jpg|bin    sprite NNN's image bytes exactly as stored (extension from magic)
  NNN.tail.bin           bytes after an empty sprite's rect count (shipped paks: u32 key + 1x1-ish
                         png placeholder the client never reads; anything else is "unknown")
  NNN.raw.bin            a whole sprite entry that did not parse (unknown)
  unknown/gap_<off>.bin  file bytes covered by no header, table entry or sprite (unknown)

Format (client reader: sources/dependencies/client/includes/PAK.h):
  file header 20 B   "<Pak file header>" (17) + 3 pad
  u32 sprite_count   (reader rejects 0 and > 1500)
  sprite_count x {u32 offset, u32 size}
  sprite at offset, `size` bytes:
    100 B header     "<Sprite File Header>" (20) + 80 pad
    u32 rect_count   (0 = empty sprite; reader rejects > 1000)
    rect_count x {u16 x, y, w, h; i16 pivot_x, pivot_y}   12 B each
    u32 colour key   (the "padding" the client skips; holds 00 R G B in file order)
    image bytes      the rest of `size` (BMP in all shipped paks)
Encrypted (Korea 2024) paks are not handled: they fail with a clear message.
"""
import hashlib
import json
import os
import re
import struct
import sys

FILE_HDR = b"<Pak file header>"
SPR_HDR = b"<Sprite File Header>"
FRAME_FIELDS = ["x", "y", "w", "h", "pivot_x", "pivot_y"]
SKIP_DIRS = {"game", "bin", "build", ".git", ".claude", "working", "node_modules", "__pycache__"}
FORMAT_VERSION = 1
TOOL_ID = "pak_extract"
STUB_NAME = ".pak_extract_stub.json"
CLEARING_NAME = ".pak_extract_clearing.json"  # mirror mode only: written before a folder is cleared
IN_PLACE_ROOTS = ("staged", "tools/display_id_manager/sprites")  # in-place write roots, under the tree
INDEX_ROOTS = ("staged/client/sprites", "tools/display_id_manager/sprites")  # each gets an index.json
# every in-place base and each folder on the way to one; all must be plain, canonical folders
BASE_CHAIN = ("staged", "tools", "tools/display_id_manager", "tools/display_id_manager/sprites")
SIDE_FILE = re.compile(r"\d{3,}\.(bmp|png|jpg|bin|tail\.bin|raw\.bin)")  # always fullmatch
GAP_FILE = re.compile(r"unknown/gap_\d+\.bin")  # always fullmatch
INDEX_LOCK_NAME = ".index.json.pak_extract_lock"  # marks a partial index.json.tmp as ours


def repo_root():
    return canon(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))


def find_paks(root):
    """(paks, issues). SKIP_DIRS apply at the top level only. Never descends into a link or junction,
    across a filesystem (st_dev) boundary, or into a folder holding a pak_extract manifest. issues lists
    every skipped link, device crossing, stray manifest and walk error; an index under any of them
    would be incomplete."""
    found, issues = [], []
    root_dev = os.stat(root).st_dev

    def on_error(err):
        issues.append((getattr(err, "filename", None) or root, "walk error: %s" % err))
    for dp, dns, fns in os.walk(root, onerror=on_error):
        keep = []
        for d in sorted(dns):
            p = os.path.join(dp, d)
            if dp == root and d.lower() in SKIP_DIRS:
                continue
            if is_link(p):
                issues.append((p, "symlink or junction skipped"))
                continue
            try:
                if os.stat(p).st_dev != root_dev:
                    issues.append((p, "other filesystem skipped"))
                    continue
            except OSError as e:
                issues.append((p, "walk error: %s" % e))
                continue
            mp = os.path.join(p, "manifest.json")
            if manifest_is_ours(mp):
                # an extracted folder only when its manifest names exactly the sibling <folder>.pak, which
                # exists; any other pak_extract manifest is a scan issue (reported, blocks the index)
                pak = os.path.join(dp, d + ".pak")
                if not (os.path.isfile(pak) and manifest_is_ours(mp, rel_posix(root, pak))):
                    issues.append((p, "pak_extract manifest without its matching sibling .pak"))
                continue  # never descend: its contents are output, never sources
            keep.append(d)
        dns[:] = keep
        for fn in sorted(fns):
            if fn.lower().endswith(".pak"):
                found.append(os.path.join(dp, fn))
    return found, issues


def rel_posix(root, path):
    return os.path.relpath(path, root).replace(os.sep, "/")


def out_dir_for(ctx, path):
    """In place: dirname(pak)/<stem>. With --out: the pak's path mirrored under the output root."""
    if not ctx["out_root"]:
        return os.path.join(os.path.dirname(path), os.path.basename(path)[:-4])
    return os.path.join(ctx["out_root"], *rel_posix(ctx["tree"], path)[:-4].split("/"))


def image_info(img):
    """Describe the image bytes without altering them."""
    info = {"size": len(img)}
    if img[:2] == b"BM":
        info["format"] = "bmp"
        if len(img) >= 54:
            declared, _r1, _r2, px_off = struct.unpack_from("<IHHI", img, 2)
            hdr = struct.unpack_from("<I", img, 14)[0]
            w, h = struct.unpack_from("<ii", img, 18)
            planes, bpp, comp = struct.unpack_from("<HHI", img, 26)
            used = struct.unpack_from("<I", img, 46)[0]
            info.update({"width": w, "height": h, "bpp": bpp, "compression": comp,
                         "dib_header_size": hdr, "pixel_data_offset": px_off,
                         "palette_colors_used_field": used,
                         "palette_colors": (used or (1 << bpp)) if bpp <= 8 else 0,
                         "bmp_declared_file_size": declared})
            if declared != len(img):
                info["note"] = "bmp declared size %d != stored size %d" % (declared, len(img))
    elif img[:8] == b"\x89PNG\r\n\x1a\n":
        info["format"] = "png"
        if len(img) >= 24:
            info["width"], info["height"] = struct.unpack_from(">II", img, 16)
            if len(img) >= 29:
                # IHDR: bit depth, colour type (0 grey, 2 rgb, 3 palette, 4 grey+a, 6 rgba), interlace
                info["bit_depth"], info["png_color_type"] = img[24], img[25]
                info["png_interlace"] = img[28]
            info["has_plte"] = b"PLTE" in img[:4096]
            info["has_trns"] = b"tRNS" in img[:4096]
    elif img[:2] == b"\xff\xd8":
        info["format"] = "jpg"
    else:
        info["format"] = "unknown"
    return info


def ext_for(fmt):
    return {"bmp": "bmp", "png": "png", "jpg": "jpg"}.get(fmt, "bin")


def parse_pak(data):
    """Parse into a model; raises ValueError for a pak this tool cannot read."""
    if data[:17] != FILE_HDR:
        raise ValueError("not a plain pak (bad signature; encrypted Korea 2024 pak?)")
    if len(data) < 24:
        raise ValueError("truncated header")
    count = struct.unpack_from("<I", data, 20)[0]
    table_end = 24 + 8 * count
    if count == 0 or count > 1500 or table_end > len(data):
        raise ValueError("bad sprite count %d" % count)
    covered = bytearray(len(data))
    covered[0:table_end] = b"\x01" * table_end
    sprites = []
    for i in range(count):
        off, size = struct.unpack_from("<II", data, 24 + 8 * i)
        sp = {"index": i, "offset": off, "size": size}
        sprites.append(sp)
        end = off + size
        if end > len(data) or size < 104 or off < table_end:
            sp["raw"] = bytes(data[off:end]) if off <= len(data) else b""
            sp["unknown"] = "entry range invalid (offset %d size %d file %d)" % (off, size, len(data))
            if off < len(data):
                for k in range(off, min(end, len(data))):
                    covered[k] = 1
            continue
        for k in range(off, end):
            covered[k] = 1
        hdr = bytes(data[off:off + 100])
        rc = struct.unpack_from("<I", data, off + 100)[0]
        if hdr[:20] != SPR_HDR:
            sp["raw"] = bytes(data[off:end])
            sp["unknown"] = "sprite header signature mismatch"
            continue
        if hdr[20:] != b"\x00" * 80:
            sp["header_padding_hex"] = hdr[20:].hex()
        sp["rect_count"] = rc
        if rc == 0:
            tail = bytes(data[off + 104:end])
            if tail:
                sp["tail"] = tail
            continue
        rect_end = off + 104 + 12 * rc
        if rc > 1000 or rect_end + 4 > end:
            sp["raw"] = bytes(data[off:end])
            sp["unknown"] = "rect count %d does not fit the entry" % rc
            sp.pop("rect_count", None)
            continue
        sp["frames"] = [list(struct.unpack_from("<HHHHhh", data, off + 104 + 12 * k)) for k in range(rc)]
        sp["key"] = struct.unpack_from("<I", data, rect_end)[0]
        sp["image"] = bytes(data[rect_end + 4:end])
    gaps = []
    i = 0
    n = len(data)
    while i < n:
        if covered[i]:
            i += 1
            continue
        j = i
        while j < n and not covered[j]:
            j += 1
        gaps.append({"offset": i, "bytes": bytes(data[i:j])})
        i = j
    return {"file_padding": bytes(data[17:20]), "count": count, "sprites": sprites,
            "gaps": gaps, "total_size": len(data)}


def write_bytes(path, b):
    with open(path, "wb") as f:
        f.write(b)


def canon(p):
    """realpath (expands 8.3 names, resolves links) with any \\\\?\\ prefix dropped (realpath keeps it)."""
    r = os.path.realpath(p)
    if r.startswith("\\\\?\\UNC\\"):
        return "\\\\" + r[8:]
    if r.startswith("\\\\?\\"):
        return r[4:]
    return r


def real(p):
    """Comparison form; path strings are only ever compared as real()."""
    return os.path.normcase(canon(p))


def inside(child, parent):
    """True when real(child) is real(parent) or below it."""
    c, p = real(child), real(parent)
    try:
        return os.path.commonpath([c, p]) == p
    except ValueError:  # different drives
        return False


def strictly_inside(child, parent):
    return inside(child, parent) and real(child) != real(parent)


def is_link(p):
    return os.path.islink(p) or os.path.isjunction(p)


def self_and_ancestors(p):
    """p, its parent, ... up to the drive or share root."""
    while True:
        yield p
        parent = os.path.dirname(p)
        if parent == p:
            return
        p = parent


def link_in_ancestry(p):
    """First component of abspath(p), p itself included, that is a symlink or junction, else None."""
    return next((a for a in self_and_ancestors(os.path.abspath(p)) if is_link(a)), None)


def is_repo_or_ancestor(path, root):
    """File-identity check: True when path is the same directory as root or any ancestor of it.
    Catches every spelling (8.3, \\\\?\\, UNC admin share, case) that string checks could miss."""
    try:
        st = os.stat(path)
    except FileNotFoundError:
        return False  # the repo and its ancestors exist, so a missing path is none of them
    return any(os.path.samestat(st, os.stat(a)) for a in self_and_ancestors(root))


def check_out_root(root, out_arg, out_raw, out_root):
    """Return an error string when out_root is not a safe place to write, else None.
    root and out_root are canon(); out_raw is abspath before resolving (links still visible);
    out_arg is --out as typed."""
    build = os.path.join(root, "build")
    if out_arg.startswith(("\\\\", "//")):
        return "refusing --out %s: \\\\?\\, device and UNC paths are not allowed" % out_arg
    if os.path.splitdrive(real(out_root))[0] != os.path.normcase(os.path.splitdrive(root)[0]):
        return "refusing --out %s: different drive from the repo" % out_arg
    try:
        if inside(root, out_root) or is_repo_or_ancestor(out_root, root):
            return "refusing --out %s: it is the repo root or an ancestor of it" % out_root
    except OSError as e:
        return "refusing --out %s: cannot check it (%s)" % (out_root, e)
    link = link_in_ancestry(out_raw)
    if link:
        return "refusing --out %s: %s is a symlink or junction" % (out_root, link)
    if not strictly_inside(out_root, build):
        return "refusing --out %s: must be strictly inside %s" % (out_root, build)
    return None


def manifest_is_ours(mp, source=None):
    """True when mp is a pak_extract manifest (or stub) for exactly this source pak. With source None,
    any pak_extract manifest counts (used to recognise an extracted folder)."""
    try:
        with open(mp, "r", encoding="utf-8") as f:
            m = json.load(f)
    except Exception:
        return False
    return (isinstance(m, dict) and m.get("tool") == TOOL_ID
            and (source is None or (m.get("format_version") == FORMAT_VERSION and m.get("source") == source)))


def base_for(ctx, path):
    """The write root path must sit strictly inside (the --out root, or in place the allowed root
    holding it), else None."""
    bases = [ctx["out_root"]] if ctx["out_root"] else ctx["bases"]
    return next((b for b in bases if strictly_inside(path, b)), None)


def check_target(ctx, path):
    """Raise unless path is a place this tool may create or remove a folder: strictly inside a write
    root, already canonical (resolves to itself, so no 8.3 / \\\\?\\ / case alias and no link), no
    symlink or junction anywhere in its ancestry, and not the repo (or --root tree) or an ancestor."""
    link = link_in_ancestry(path)
    if link:
        raise ValueError("refusing %s: %s is a symlink or junction" % (path, link))
    if base_for(ctx, path) is None:
        raise ValueError("refusing %s: not strictly inside an allowed write root" % path)
    if real(path) != os.path.normcase(path):
        raise ValueError("refusing %s: it resolves to %s" % (path, canon(path)))
    for r in ctx["guarded"]:
        if is_repo_or_ancestor(path, r):
            raise ValueError("refusing %s: it is the repo root or an ancestor of it" % path)


def is_side_file(rel):
    """True for a name this tool writes beside manifest.json (NNN.<ext>, NNN.tail.bin, NNN.raw.bin,
    unknown/gap_<off>.bin)."""
    return bool(SIDE_FILE.fullmatch(rel) or GAP_FILE.fullmatch(rel))


def entries_of(folder):
    """{relative posix path: "dir" | "file"} for everything below folder, links never followed;
    raise ValueError on a link, a walk error or anything that is neither a plain file nor a folder."""
    found = {}

    def fail(err):
        raise ValueError("refusing %s: cannot list it (%s)" % (folder, err))
    for dp, dns, fns in os.walk(folder, onerror=fail):
        for n in dns + fns:
            p = os.path.join(dp, n)
            if is_link(p):
                raise ValueError("refusing %s: %s is a symlink or junction" % (folder, p))
            if n not in dns and not os.path.isfile(p):
                raise ValueError("refusing %s: %s is not a plain file" % (folder, p))
            found[rel_posix(folder, p)] = "dir" if n in dns else "file"
    return found


def mirror_clearable(ctx, path, source, is_tmp):
    """Mirror mode only. Return the files to delete (manifest or stub last) when path is provably this
    tool's own output for source, None when it does not exist; raise ValueError otherwise. A final
    folder must hold exactly manifest.json plus the side files its manifest lists (and unknown/ when
    it lists gaps): no .pak, no link, nothing extra. A .tmp must hold our stub and only side files
    (plus our manifest when the extract got that far)."""
    if not ctx["out_root"]:
        raise ValueError("internal: mirror_clearable called in in-place mode")
    if not os.path.lexists(path):
        return None
    check_target(ctx, path)
    if not os.path.isdir(path):
        raise ValueError("refusing %s: it exists and is not a folder; left untouched" % path)
    ents = entries_of(path)
    files = {k for k, v in ents.items() if v == "file"}
    dirs = {k for k, v in ents.items() if v == "dir"}
    has_manifest = manifest_is_ours(os.path.join(path, "manifest.json"), source)
    clearing = CLEARING_NAME in files and manifest_is_ours(os.path.join(path, CLEARING_NAME), source)
    if is_tmp:
        # ours by the stub, or by our manifest once the stub has been dropped just before the rename
        stub = STUB_NAME in files and manifest_is_ours(os.path.join(path, STUB_NAME), source)
        ok = (stub or has_manifest) and dirs <= {"unknown"} and all(
            is_side_file(f) or (f == STUB_NAME and stub) or (f == "manifest.json" and has_manifest)
            for f in files)
    elif has_manifest:
        try:
            listed = manifest_side_files(os.path.join(path, "manifest.json"))
        except Exception:  # a malformed manifest refuses this pak only
            listed = None
        want_dirs = {"unknown"} if listed and any(n.startswith("unknown/") for n in listed) else set()
        if listed is None:
            ok = False
        elif clearing:  # an earlier clear was interrupted: any subset of what it was deleting
            ok = files - {"manifest.json", CLEARING_NAME} <= listed and dirs <= want_dirs
        else:
            ok = files == listed | {"manifest.json"} and dirs == want_dirs
            if ok:  # contents too: an edited image no longer rebuilds to source_sha256
                try:
                    rebuild_pak(path)
                except Exception:
                    ok = False
    else:  # interrupted after the manifest went: only the clearing marker (and an empty unknown/) is left
        ok = clearing and files == {CLEARING_NAME} and dirs <= {"unknown"}
    if not ok or any(f.lower().endswith(".pak") for f in files):
        raise ValueError("refusing %s: it is not exactly pak_extract output for %s (extra, missing or "
                         "edited entries); left untouched" % (path, source))
    last = [f for f in ("manifest.json", STUB_NAME, CLEARING_NAME) if f in files]
    return sorted(f for f in files if f not in last) + last


def manifest_side_files(mp):
    """The side files a manifest lists, as a set; raise when the manifest's shape is wrong."""
    with open(mp, "r", encoding="utf-8") as fh:
        m = json.load(fh)
    listed = set()
    for e in m["sprites"]:
        for name in (e.get("raw_file"), e.get("tail_file"), (e.get("image") or {}).get("file")):
            if name is not None:
                listed.add(name)
    listed |= {g["file"] for g in m["unknown_gaps"]}
    if not all(isinstance(n, str) and is_side_file(n) for n in listed):
        raise ValueError("manifest lists a name this tool does not write")
    return listed


def clear_mirror(ctx, path, source, is_tmp=False):
    """Mirror mode only: re-check, mark the folder as being cleared (a final folder only), then delete
    file by file (manifest, then stub / marker last), then the folders. No rmtree: nothing outside the
    checked list can be removed, and an interrupted clear is recognised by its marker next run."""
    order = mirror_clearable(ctx, path, source, is_tmp)
    if order is None:
        return
    if not is_tmp and CLEARING_NAME not in order:
        with open(os.path.join(path, CLEARING_NAME), "w", encoding="utf-8", newline="\n") as f:
            json.dump({"tool": TOOL_ID, "format_version": FORMAT_VERSION, "source": source,
                       "note": "being cleared by pak_extract"}, f)
        order.append(CLEARING_NAME)
    for rel in order:
        os.remove(os.path.join(path, *rel.split("/")))
    if os.path.isdir(os.path.join(path, "unknown")):
        os.rmdir(os.path.join(path, "unknown"))
    os.rmdir(path)


def extract_pak(ctx, path):
    """Extract into a fresh <out>.tmp (marked by a stub), then rename it to <out>, so an interrupted
    run never leaves a half-written folder at <out>. In place, an existing <out> or <out>.tmp refuses
    the pak and nothing is ever deleted or replaced; in mirror mode our own exact output is cleared.
    The .pak itself is only ever read."""
    with open(path, "rb") as f:
        data = f.read()
    model = parse_pak(data)
    source = rel_posix(ctx["tree"], path)
    final = out_dir_for(ctx, path)
    tmp = final + ".tmp"
    check_target(ctx, final)
    check_target(ctx, tmp)
    if ctx["out_root"]:
        replacing = mirror_clearable(ctx, final, source, False) is not None  # foreign: raises, nothing written
        clear_mirror(ctx, tmp, source, is_tmp=True)  # leftover of an interrupted run
    else:
        for p in (final, tmp):
            if os.path.lexists(p):
                raise ValueError("refusing %s: exists; in-place mode never overwrites" % p)
        replacing = False
    os.makedirs(tmp)
    with open(os.path.join(tmp, STUB_NAME), "w", encoding="utf-8", newline="\n") as f:
        json.dump({"tool": TOOL_ID, "format_version": FORMAT_VERSION, "source": source,
                   "note": "in-progress extract; safe to delete"}, f)
    out = tmp
    manifest = {
        "tool": TOOL_ID,
        "format_version": FORMAT_VERSION,
        "source": source,
        "source_size": len(data),
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "file_header": {"signature": FILE_HDR.decode("ascii"),
                        "padding_hex": model["file_padding"].hex()},
        "sprite_count": model["count"],
        "frame_fields": FRAME_FIELDS,
        "color_key_note": "color_key_u32 is the 4 bytes after the frame table; file order is 00 R G B "
                          "(color_key_rgb = R=(k>>8)&255, G=(k>>16)&255, B=(k>>24)&255, derived)",
        "sprites": [],
        "unknown_gaps": [],
    }
    stats = {"unknown": 0}
    for sp in model["sprites"]:
        i = sp["index"]
        e = {"index": i, "offset": sp["offset"], "size": sp["size"]}
        if "raw" in sp:
            e["unknown"] = sp["unknown"]
            e["raw_file"] = "%03d.raw.bin" % i
            write_bytes(os.path.join(out, e["raw_file"]), sp["raw"])
            stats["unknown"] += 1
        else:
            if "header_padding_hex" in sp:
                e["header_padding_hex"] = sp["header_padding_hex"]
                e["unknown_header_padding"] = True
                stats["unknown"] += 1
            e["rect_count"] = sp["rect_count"]
            if sp["rect_count"] == 0:
                e["empty"] = True
                if "tail" in sp:
                    t = sp["tail"]
                    e["tail_file"] = "%03d.tail.bin" % i
                    e["tail_size"] = len(t)
                    write_bytes(os.path.join(out, e["tail_file"]), t)
                    if t[4:12] == b"\x89PNG\r\n\x1a\n":
                        # an empty sprite still carries a u32 key and a tiny placeholder image
                        # that the client never reads (rect_count 0 skips them)
                        w, h = struct.unpack_from(">II", t, 20) if len(t) >= 28 else (0, 0)
                        e["tail_layout"] = "u32 colour key %08x + png placeholder %dx%d (unused by the client)" \
                            % (struct.unpack_from("<I", t, 0)[0], w, h)
                    else:
                        e["unknown_tail"] = True
                        stats["unknown"] += 1
            else:
                k = sp["key"]
                e["frames"] = sp["frames"]
                e["color_key_u32"] = k
                e["color_key_hex"] = "%08x" % k
                e["color_key_rgb"] = [(k >> 8) & 255, (k >> 16) & 255, (k >> 24) & 255]
                info = image_info(sp["image"])
                info["file"] = "%03d.%s" % (i, ext_for(info["format"]))
                e["image"] = info
                write_bytes(os.path.join(out, info["file"]), sp["image"])
                if info["format"] == "unknown":
                    e["unknown"] = "image bytes are neither BMP, PNG nor JPEG"
                    stats["unknown"] += 1
        manifest["sprites"].append(e)
    if model["gaps"]:
        os.makedirs(os.path.join(out, "unknown"))
    for g in model["gaps"]:
        fn = "unknown/gap_%d.bin" % g["offset"]
        write_bytes(os.path.join(out, *fn.split("/")), g["bytes"])
        manifest["unknown_gaps"].append({"offset": g["offset"], "size": len(g["bytes"]), "file": fn,
                                         "unknown": "file bytes covered by no header, table entry or sprite"})
        stats["unknown"] += 1
    manifest["unknown_count"] = stats["unknown"]
    with open(os.path.join(out, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=1)
        f.write("\n")
    os.remove(os.path.join(tmp, STUB_NAME))
    if replacing:
        clear_mirror(ctx, final, source)
    if os.path.lexists(final):  # rename never overwrites: on POSIX it would silently replace an empty dir
        raise ValueError("refusing %s: it appeared during the extract; %s left in place" % (final, tmp))
    os.rename(tmp, final)
    return len(data), manifest, stats


def rebuild_pak(folder):
    """Rebuild the pak bytes from manifest.json and the side files only."""
    with open(os.path.join(folder, "manifest.json"), "r", encoding="utf-8") as f:
        m = json.load(f)
    buf = bytearray(m["source_size"])
    sig = m["file_header"]["signature"].encode("ascii")
    buf[0:17] = sig
    buf[17:20] = bytes.fromhex(m["file_header"]["padding_hex"])
    count = m["sprite_count"]
    if count != len(m["sprites"]):
        raise ValueError("sprite_count does not match the sprite list")
    struct.pack_into("<I", buf, 20, count)

    def rd(name):
        parts = name.split("/")
        if (not name or "\\" in name or ":" in name or name.startswith("/")
                or any(p in ("", ".", "..") for p in parts) or len(parts) > 2
                or (len(parts) == 2 and parts[0] != "unknown")):
            raise ValueError("unsafe side-file name in manifest: %r" % name)
        with open(os.path.join(folder, *parts), "rb") as fh:
            return fh.read()

    for e in m["sprites"]:
        i, off, size = e["index"], e["offset"], e["size"]
        struct.pack_into("<II", buf, 24 + 8 * i, off, size)
        if "raw_file" in e:
            part = rd(e["raw_file"])
        else:
            pad = bytes.fromhex(e["header_padding_hex"]) if "header_padding_hex" in e else b"\x00" * 80
            part = bytearray(SPR_HDR + pad)
            part += struct.pack("<I", e["rect_count"])
            if e["rect_count"] == 0:
                if "tail_file" in e:
                    part += rd(e["tail_file"])
            else:
                for fr in e["frames"]:
                    part += struct.pack("<HHHHhh", *fr)
                part += struct.pack("<I", e["color_key_u32"])
                part += rd(e["image"]["file"])
            part = bytes(part)
        if len(part) != size:
            raise ValueError("sprite %d rebuilt to %d bytes, table says %d" % (i, len(part), size))
        buf[off:off + size] = part
    for g in m["unknown_gaps"]:
        b = rd(g["file"])
        buf[g["offset"]:g["offset"] + len(b)] = b
    out = bytes(buf)
    want = m.get("source_sha256")
    if not want:
        raise ValueError("manifest has no source_sha256")
    if hashlib.sha256(out).hexdigest() != want:
        raise ValueError("rebuilt bytes do not match the manifest source_sha256")
    return out


def human(n):
    for u in ("B", "KB", "MB", "GB"):
        if n < 1024 or u == "GB":
            return "%.1f %s" % (n, u) if u != "B" else "%d B" % n
        n /= 1024.0


def folder_size(folder):
    total = 0
    for dp, _dn, fns in os.walk(folder):
        for fn in fns:
            total += os.path.getsize(os.path.join(dp, fn))
    return total


def parse_args(argv):
    opts = {"dry": False, "verify": False, "only": [], "out": None, "root": None}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--dry-run":
            opts["dry"] = True
        elif a == "--verify":
            opts["verify"] = True
        elif a in ("--out", "--root"):
            i += 1
            if i >= len(argv) or argv[i].startswith("--") or not argv[i]:
                raise SystemExit("usage error: %s needs a directory" % a)
            opts[a[2:]] = argv[i]
        elif a == "--only":
            i += 1
            start = len(opts["only"])
            while i < len(argv) and not argv[i].startswith("--"):
                opts["only"].append(argv[i].lower())
                i += 1
            if len(opts["only"]) == start:
                raise SystemExit("usage error: --only needs at least one name")
            continue
        else:
            raise SystemExit("unknown argument: " + a)
        i += 1
    return opts


def asc(s):
    return str(s).encode("ascii", "replace").decode()


def write_index(ctx, all_paks, issues, sub):
    """Write <tree>/<sub>/index.json naming every extracted folder below it, but only when the scan was
    complete (no walk error anywhere, no link or device boundary skipped under <sub>) and every pak
    under <sub> rebuilds byte-identical from its folder; raise ValueError (and write nothing) otherwise.
    Returns the asset count, or None when the tree has no paks there."""
    base = os.path.join(ctx["tree"], *sub.split("/"))
    nb = os.path.normcase(base)

    def under(p):  # lexical on purpose: real() would resolve the very link being reported
        try:
            return os.path.commonpath([os.path.normcase(os.path.abspath(p)), nb]) == nb
        except ValueError:
            return False
    gaps = [(p, why) for p, why in issues if why.startswith("walk error") or under(p)]
    if gaps:
        raise ValueError("scan incomplete (%s: %s)" % gaps[0])
    mine = [p for p in all_paks if strictly_inside(p, base)]
    if not mine:
        return None
    assets = {}
    for p in mine:
        folder = out_dir_for(ctx, p)
        if not manifest_is_ours(os.path.join(folder, "manifest.json"), rel_posix(ctx["tree"], p)):
            raise ValueError("%s has no pak_extract folder" % rel_posix(ctx["tree"], p))
        with open(p, "rb") as f:
            if rebuild_pak(folder) != f.read():
                raise ValueError("%s does not rebuild byte-identical" % rel_posix(ctx["tree"], p))
        with open(os.path.join(folder, "manifest.json"), "r", encoding="utf-8") as f:
            count = json.load(f)["sprite_count"]
        key = rel_posix(base, p)[:-4]  # the name the client asks for, e.g. "npcs/ogre"
        assets[key] = {"dir": key, "sprite_count": count, "manifest": key + "/manifest.json"}
    idx = os.path.join(base, "index.json")
    check_target(ctx, idx)
    if os.path.lexists(idx):
        try:
            with open(idx, "r", encoding="utf-8") as f:
                prev = json.load(f)
        except Exception:
            prev = None
        if not os.path.isfile(idx) or not isinstance(prev, dict) or prev.get("tool") != TOOL_ID:
            raise ValueError("refusing to overwrite %s: not a pak_extract index" % idx)
    # The index keeps its on-disk key order (sorted: assets, format_version, tool), so a regeneration
    # makes no diff; the ownership of a partial index.json.tmp is therefore marked by a lock file of ours,
    # written before the .tmp and removed after the swap (a leftover pair means an interrupted write).
    part, lock = idx + ".tmp", os.path.join(base, INDEX_LOCK_NAME)
    lock_id = sub + "/index.json"
    check_target(ctx, part)
    check_target(ctx, lock)
    lock_ours = os.path.isfile(lock) and manifest_is_ours(lock, lock_id)
    if os.path.lexists(lock) and not lock_ours:
        raise ValueError("refusing: %s exists and is not a pak_extract index lock" % lock)
    if os.path.lexists(part):
        if not (lock_ours and os.path.isfile(part)):
            raise ValueError("refusing: %s exists and is not a partial pak_extract index" % part)
        os.remove(part)
    if not lock_ours:
        with open(lock, "w", encoding="utf-8", newline="\n") as f:
            json.dump({"tool": TOOL_ID, "format_version": FORMAT_VERSION, "source": lock_id,
                       "note": "index write in progress"}, f)
    with open(part, "w", encoding="utf-8", newline="\n") as f:
        json.dump({"format_version": FORMAT_VERSION, "tool": TOOL_ID, "assets": assets}, f,
                  indent=1, sort_keys=True)
        f.write("\n")
    os.replace(part, idx)  # a file, regenerated from the folders; never a folder
    os.remove(lock)
    return len(assets)


def check_tree_root(repo, tree_raw, tree):
    """--root (a scratch source tree for tests) must be an existing folder strictly inside <repo>/build."""
    if not os.path.isdir(tree):
        return "refusing --root %s: not an existing folder" % tree_raw
    link = link_in_ancestry(tree_raw)
    if link:
        return "refusing --root %s: %s is a symlink or junction" % (tree_raw, link)
    if not strictly_inside(tree, os.path.join(repo, "build")) or is_repo_or_ancestor(tree, repo):
        return "refusing --root %s: must be strictly inside %s" % (tree_raw, os.path.join(repo, "build"))
    return None


def clash_report(ctx, paks):
    """(blocking, per_pak) problems found before any write. blocking: two paks mapping to one folder,
    or a pak's <folder>.tmp being another pak's folder. per_pak: target folders that already exist
    (ours / not ours) and targets outside the write roots; each refuses only its own pak."""
    finals = {}
    for p in paks:
        finals.setdefault(os.path.normcase(out_dir_for(ctx, p)), []).append(p)
    blocking = ["same folder %s <- %s" % (out_dir_for(ctx, v[0]), ", ".join(rel_posix(ctx["tree"], p) for p in v))
                for k, v in sorted(finals.items()) if len(v) > 1]
    blocking += ["%s.tmp is also the folder of %s" % (out_dir_for(ctx, finals[k][0]),
                                                      rel_posix(ctx["tree"], finals[k + ".tmp"][0]))
                 for k in sorted(finals) if k + ".tmp" in finals]
    per_pak = []
    for p in paks:
        source, final = rel_posix(ctx["tree"], p), out_dir_for(ctx, p)
        for path, is_tmp, label in ((final, False, "folder"), (final + ".tmp", True, "leftover .tmp")):
            if base_for(ctx, path) is None:
                per_pak.append("%s: %s %s is outside the write roots (will refuse)" % (source, label, path))
                break
            if not os.path.lexists(path):
                continue
            if not ctx["out_root"]:
                per_pak.append("%s: %s %s exists; in-place mode never overwrites (will refuse)"
                               % (source, label, path))
                continue
            try:
                mirror_clearable(ctx, path, source, is_tmp)
                per_pak.append("%s: %s exists, exactly our own output (will be replaced)" % (source, label))
            except Exception as e:  # a malformed folder or manifest refuses this pak only
                per_pak.append("%s: %s (will refuse this pak)" % (source, e))
    return blocking, per_pak


def check_bases(ctx):
    """Every in-place base and every folder on the way to one must be a plain, canonical folder."""
    for b in BASE_CHAIN:
        path = os.path.join(ctx["tree"], *b.split("/"))
        link = link_in_ancestry(path)
        if link:
            return "refusing write root %s: %s is a symlink or junction" % (path, link)
        if real(path) != os.path.normcase(path):
            return "refusing write root %s: it resolves to %s" % (path, canon(path))
    return None


def main(argv):
    if sys.version_info < (3, 12):
        print("pak_extract needs Python 3.12 or newer (os.path.isjunction); this is %d.%d"
              % sys.version_info[:2])
        return 2
    try:
        opts = parse_args(argv)
    except SystemExit as e:  # usage errors exit 2 like every other refusal
        print(asc(e))
        return 2
    repo = repo_root()
    tree = repo
    if opts["root"]:
        tree_raw = os.path.abspath(opts["root"])
        tree = canon(tree_raw)
        err = check_tree_root(repo, tree_raw, tree)
        if err:
            print(asc(err))
            return 2
    ctx = {"tree": tree, "out_root": None, "guarded": [repo] + ([tree] if tree != repo else []),
           "bases": [os.path.join(tree, *b.split("/")) for b in IN_PLACE_ROOTS]}
    err = check_bases(ctx)  # bases are used as plain strings below, so they must already be canonical
    if err:
        print(asc(err))
        return 2
    if opts["out"]:
        # out_root is resolved once here; every later path is built from this resolved form
        out_raw = os.path.abspath(opts["out"])
        ctx["out_root"] = canon(out_raw)
        err = check_out_root(repo, opts["out"], out_raw, ctx["out_root"])
        if err:
            print(asc(err))
            return 2
    all_paks, issues = find_paks(tree)
    paks = all_paks
    if opts["only"]:
        def wanted(p):
            rel = rel_posix(tree, p).lower()
            base = os.path.basename(rel)[:-4]
            return any(o == base or o == rel or o == rel[:-4] for o in opts["only"])
        paks = [p for p in paks if wanted(p)]
    total_in = sum(os.path.getsize(p) for p in paks)
    blocking, per_pak = clash_report(ctx, paks) if not opts["verify"] else ([], [])

    def shown(path):
        try:
            return rel_posix(tree, path)
        except ValueError:  # another drive or mount: show it absolute
            return path

    if opts["dry"]:
        for p in paks:
            print(asc("%12d  %s -> %s" % (os.path.getsize(p), rel_posix(tree, p), shown(out_dir_for(ctx, p)) + "/")))
        # raw images are stored as is, so output ~ input + a manifest (est. 40 B per frame, ~300 B per sprite)
        print("paks=%d input=%s (%d bytes)" % (len(paks), human(total_in), total_in))
        print("estimated output ~ %s (images verbatim plus manifests); nothing written" % human(int(total_in * 1.02)))
        print(asc("mode: " + ("mirror under " + ctx["out_root"] if ctx["out_root"] else
                              "in place, write roots " + ", ".join(ctx["bases"]))))
        print("clashes: %d blocking, %d per-pak; scan issues: %d" % (len(blocking), len(per_pak), len(issues)))
        for p, why in issues:
            print(asc("  scan: %s: %s" % (p, why)))
        for c in blocking:
            print(asc("  BLOCKING: " + c))
        for c in per_pak:
            print(asc("  " + c))
        return 2 if blocking else 0
    if blocking:
        for c in blocking:
            print(asc("refusing, nothing written: " + c))
        return 2

    ok = bad = 0
    failures = []
    unknown_report = []
    written = []
    for n, p in enumerate(paks, 1):
        rel = rel_posix(tree, p)
        try:
            if opts["verify"]:
                folder = out_dir_for(ctx, p)
                with open(p, "rb") as f:
                    orig = f.read()
                rebuilt = rebuild_pak(folder)
                if rebuilt == orig:
                    ok += 1
                else:
                    bad += 1
                    first = next((k for k in range(min(len(rebuilt), len(orig))) if rebuilt[k] != orig[k]),
                                 min(len(rebuilt), len(orig)))
                    failures.append("%s: MISMATCH (orig %d B, rebuilt %d B, first diff at %d)"
                                    % (rel, len(orig), len(rebuilt), first))
            else:
                _size, _manifest, stats = extract_pak(ctx, p)
                written.append(out_dir_for(ctx, p))
                ok += 1
                if stats["unknown"]:
                    unknown_report.append("%s: %d unknown item(s)" % (rel, stats["unknown"]))
        except Exception as e:
            bad += 1
            failures.append("%s: %s" % (rel, e))
        if n % 25 == 0:
            print("progress %d/%d" % (n, len(paks)))
            sys.stdout.flush()
    if opts["verify"]:
        print("verify: %d of %d byte-identical, %d failed" % (ok, len(paks), bad))
    else:
        out_bytes = sum(folder_size(d) for d in written)
        print("extracted=%d failed=%d input=%s (%d B) output=%s (%d B)"
              % (ok, bad, human(total_in), total_in, human(out_bytes), out_bytes))
        for u in unknown_report:
            print(asc("  unknown: " + u))
    for f in failures:
        print(asc("  " + f))
    for p, why in issues:
        print(asc("  scan: %s: %s" % (p, why)))
    if not opts["verify"] and not ctx["out_root"]:
        for sub in INDEX_ROOTS:
            if bad:
                print("index %s/index.json NOT written: %d pak(s) failed in this run" % (sub, bad))
                continue
            try:
                n = write_index(ctx, all_paks, issues, sub)
                if n is not None:
                    print("index %s/index.json: %d assets" % (sub, n))
            except Exception as e:
                bad += 1
                print(asc("index %s/index.json NOT written: %s" % (sub, e)))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
