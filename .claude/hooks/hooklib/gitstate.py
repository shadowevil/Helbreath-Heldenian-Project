"""Git and disk look-ups for rule A1: the blob hash, what HEAD tracks, what the archive holds, and the
state of each tracked file on disk. Read-only; callers fail open."""
import fnmatch
import hashlib
import os
import subprocess

MAX_ENTRIES = 300
GIT_TIMEOUT_S = 8


def blob_sha(data):
    """The git blob id of `data` (what `git hash-object` prints)."""
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def _git(root, *args):
    r = subprocess.run(["git", "-C", root] + list(args), capture_output=True, timeout=GIT_TIMEOUT_S)
    return r.stdout if r.returncode == 0 else None


def head_entries(root, rels):
    """([(path, blob_sha, size)], truncated): the files HEAD tracks under the given paths (dirs
    expand; a glob in the last segment is matched against its directory's files). `truncated` is
    True when MAX_ENTRIES cut the list short."""
    out = {}
    for rel in rels:
        spec, pat = rel, None
        if any(c in rel.rsplit("/", 1)[-1] for c in "*?["):
            spec, pat = (rel.rsplit("/", 1) + [""])[0], rel.rsplit("/", 1)[-1]
        raw = _git(root, "ls-tree", "-r", "-l", "-z", "HEAD", "--", spec)
        for item in (raw or b"").split(b"\0"):
            if not item or b"\t" not in item:
                continue
            meta, path = item.split(b"\t", 1)
            parts = meta.split()
            if len(parts) < 4 or parts[1] != b"blob":
                continue
            p = path.decode("utf-8", "replace")
            if pat and not fnmatch.fnmatch(p.rsplit("/", 1)[-1].lower(), pat.lower()):
                continue
            if p not in out and len(out) >= MAX_ENTRIES:
                return list(out.values()), True
            out[p] = (p, parts[2].decode(), int(parts[3]))
    return list(out.values()), False


def archive_hashes(root, sizes):
    """Blob hashes of the files under working/archive/ whose size is one of `sizes`."""
    found = set()
    for dp, _dn, fn in os.walk(os.path.join(root, "working", "archive")):
        for f in fn:
            fp = os.path.join(dp, f)
            try:
                if os.path.getsize(fp) in sizes:
                    with open(fp, "rb") as fh:
                        found.add(blob_sha(fh.read()))
            except OSError:
                pass
    return found


def inspect(root, rels):
    """([{path, state, archived}], truncated) for each tracked file under `rels`: state is
    deleted / changed / unchanged against HEAD, archived is whether HEAD's bytes sit in the archive."""
    ents, truncated = head_entries(root, rels)
    if not ents:
        return [], truncated
    arch = archive_hashes(root, {e[2] for e in ents})
    out = []
    for path, sha, _size in ents:
        fp = os.path.join(root, *path.split("/"))
        if not os.path.exists(fp):
            state = "deleted"
        else:
            try:
                with open(fp, "rb") as fh:
                    state = "unchanged" if blob_sha(fh.read()) == sha else "changed"
            except OSError:
                state = "unchanged"
        out.append({"path": path, "state": state, "archived": sha in arch})
    return out, truncated
