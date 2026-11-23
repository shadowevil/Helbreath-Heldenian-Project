"""Download every .pak in Original-Helbreath/History sprites/korea-2024.

Usage: python -I fetch_korea_2024.py <listing.json> <out_dir>
listing.json is the output of:
  gh api "repos/Original-Helbreath/History/contents/sprites/korea-2024?ref=master"
Resumable: a file that already exists with the API size is skipped.
ASCII output only.
"""
import json
import os
import sys
import urllib.parse
import urllib.request


def main():
    listing_path, out_dir = sys.argv[1], sys.argv[2]
    with open(listing_path, "r", encoding="utf-8") as f:
        items = [i for i in json.load(f) if i.get("type") == "file" and i["name"].lower().endswith(".pak")]
    os.makedirs(out_dir, exist_ok=True)
    done = skipped = failed = 0
    total = 0
    for n, it in enumerate(items, 1):
        name = os.path.basename(it["name"])
        size = it["size"]
        dest = os.path.join(out_dir, name)
        if os.path.isfile(dest) and os.path.getsize(dest) == size:
            skipped += 1
            total += size
            continue
        url = it["download_url"]
        part = dest + ".part"
        ok = False
        for attempt in range(3):
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "pak-fetch"})
                with urllib.request.urlopen(req, timeout=120) as r, open(part, "wb") as o:
                    while True:
                        b = r.read(1 << 20)
                        if not b:
                            break
                        o.write(b)
                if os.path.getsize(part) == size:
                    os.replace(part, dest)
                    ok = True
                    break
                print("SIZE MISMATCH %s got %d want %d (try %d)" % (name, os.path.getsize(part), size, attempt + 1))
            except Exception as e:
                print("ERROR %s: %s (try %d)" % (name, str(e).encode("ascii", "replace").decode(), attempt + 1))
        if ok:
            done += 1
            total += size
        else:
            failed += 1
            print("FAILED %s" % name)
        if n % 25 == 0:
            print("progress %d/%d" % (n, len(items)))
            sys.stdout.flush()
    print("files=%d downloaded=%d skipped=%d failed=%d bytes_verified=%d" % (len(items), done, skipped, failed, total))


main()
