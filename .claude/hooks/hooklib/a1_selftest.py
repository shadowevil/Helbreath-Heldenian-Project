"""End-to-end self-test for rule A1 in a throwaway git repo under hooks/log/selftest_a1/.
run(hooks_dir, handle, kill_env) -> [(name, ok, detail)]. Cleans up after itself, read-only
git objects included."""
import os
import shutil
import stat
import subprocess

from hooklib import gitstate, output


def rmtree_force(path):
    """rmtree that survives Windows read-only files (git objects): chmod and retry."""
    def onerror(func, p, _exc):
        try:
            os.chmod(p, stat.S_IWRITE)
            func(p)
        except OSError:
            pass
    shutil.rmtree(path, onerror=onerror)


def _commit(repo):
    subprocess.run(["git", "-C", repo, "-c", "user.name=t", "-c", "user.email=t@t", "-c",
                    "commit.gpgsign=false", "commit", "-q", "-m", "c", "--allow-empty"],
                   capture_output=True, timeout=30)


def run(hooks_dir, handle, kill_env):
    results = [("blob_sha matches git", gitstate.blob_sha(b"hello\n") == "ce013625030ba8dba906f756967f9e9ca394464a", "")]
    base = os.path.join(hooks_dir, "log", "selftest_a1")
    if os.path.exists(base):
        rmtree_force(base)
    try:
        spr = os.path.join(base, "staged", "client", "sprites", "interface")
        arc = os.path.join(base, "working", "archive")
        os.makedirs(spr)
        os.makedirs(arc)
        files = {"a.png": b"lowres-a", "b.png": b"lowres-b", "c.png": b"lowres-c", "d.png": b"lowres-d",
                 "data.json": b"{}"}
        for n, d in files.items():
            with open(os.path.join(spr, n), "wb") as fh:
                fh.write(d)
        subprocess.run(["git", "-C", base, "init", "-q"], capture_output=True, timeout=30)
        subprocess.run(["git", "-C", base, "add", "-A"], capture_output=True, timeout=30)
        _commit(base)
        with open(os.path.join(arc, "old_b.png"), "wb") as fh:  # b is archived under a different name
            fh.write(files["b.png"])
        hits = os.path.join(base, "hits.jsonl")
        argv = ["--hits-path=" + hits]
        ab = base.replace("\\", "/")

        def ev(cmd, tool="Bash", sid="s1"):
            return {"tool_name": tool, "session_id": sid, "cwd": ab, "tool_input": {"command": cmd}}

        def nrows():
            return len(output.recent_rows(hits))

        os.remove(os.path.join(spr, "a.png"))
        handle(ev("rm staged/client/sprites/interface/a.png"), argv, {}, root=ab)
        r = output.recent_rows(hits)
        results.append(("e2e unarchived rm logged", len(r) == 1 and r[0]["files"] == ["staged/client/sprites/interface/a.png"]
                        and r[0]["verbs"] == ["delete"] and r[0]["flagged"] is False and r[0]["truncated"] is False, repr(r)))
        os.remove(os.path.join(spr, "b.png"))
        n0 = nrows()
        handle(ev("mv staged/client/sprites/interface/b.png working/archive/old_b.png"), argv, {}, root=ab)
        results.append(("e2e archived (by hash) silent", nrows() == n0, ""))
        with open(os.path.join(spr, "c.png"), "wb") as fh:
            fh.write(b"HD-c")
        handle(ev("cp working/classic_ui/hd/c.png staged/client/sprites/interface/c.png"), argv, {}, root=ab)
        r = output.recent_rows(hits)[-1]
        results.append(("e2e overwrite from hd logged", nrows() == n0 + 1 and r["src_hd"] is True
                        and r["states"] == ["changed"], repr(r)))
        n1 = nrows()
        handle(ev("rm staged/client/sprites/interface/d.png"), argv, {}, root=ab)
        results.append(("e2e command that changed nothing silent", nrows() == n1, ""))
        os.remove(os.path.join(spr, "data.json"))
        handle(ev("rm staged/client/sprites/interface/data.json"), argv, {}, root=ab)
        results.append(("e2e non-art file silent", nrows() == n1, ""))
        with open(os.path.join(spr, "new.png"), "wb") as fh:
            fh.write(b"x")
        handle(ev("cp x.png staged/client/sprites/interface/new.png"), argv, {}, root=ab)
        results.append(("e2e untracked file silent", nrows() == n1, ""))
        os.remove(os.path.join(spr, "d.png"))
        handle(ev("rm -r staged/client/sprites/interface"), argv, {}, root=ab)
        r = output.recent_rows(hits)[-1]
        results.append(("e2e dir delete expands", nrows() == n1 + 1 and "staged/client/sprites/interface/d.png" in r["files"]
                        and r["n_unarchived"] >= 3 and r["truncated"] is False, repr(r)))
        n2 = nrows()
        handle(ev("rm staged/client/sprites/interface/*.png"), argv, {}, root=ab)
        results.append(("e2e glob expands", nrows() == n2 + 1, ""))
        # truncation: a cap of 2 over 4 tracked art files logs truncated=True
        n_t = nrows()
        old_cap = gitstate.MAX_ENTRIES
        gitstate.MAX_ENTRIES = 2
        try:
            handle(ev("rm -r staged/client/sprites/interface"), argv, {}, root=ab)
        finally:
            gitstate.MAX_ENTRIES = old_cap
        r = output.recent_rows(hits)[-1]
        results.append(("e2e truncation flagged", nrows() == n_t + 1 and r["truncated"] is True
                        and r["n_touched"] <= 2, repr(r)))
        n3 = nrows()
        o1 = handle(ev("rm staged/client/sprites/interface/a.png", sid="f1"), ["--flag"] + argv, {}, root=ab)
        o2 = handle(ev("rm staged/client/sprites/interface/a.png", sid="f1"), ["--flag"] + argv, {}, root=ab)
        o3 = handle(ev("rm staged/client/sprites/interface/a.png", sid="f2"), ["--flag"] + argv, {kill_env: "log"}, root=ab)
        results.append(("e2e flag once, kill switch", '"additionalContext"' in o1 and o2 == "" and o3 == ""
                        and nrows() == n3 + 3, repr((o1[:40], o2, o3))))
        n4 = nrows()
        wev = {"tool_name": "Write", "session_id": "s9", "cwd": ab,
               "tool_input": {"file_path": os.path.join(spr, "c.png")}}
        handle(wev, argv, {}, root=ab)
        results.append(("e2e Write tool path checked", nrows() == n4 + 1, ""))
        results.append(("e2e garbage fails open", handle({"tool_name": "Bash", "tool_input": None}, argv, {}) == ""
                        and handle({"tool_name": "Bash", "tool_input": {"command": 5}}, argv, {}) == "", ""))
        nogit = os.path.join(base, "nogit")
        os.makedirs(nogit)
        n5 = nrows()
        handle({"tool_name": "Bash", "cwd": nogit.replace("\\", "/"), "tool_input": {
            "command": "rm staged/client/sprites/a.png"}}, argv, {}, root=nogit.replace("\\", "/"))
        results.append(("e2e no repo silent", nrows() == n5, ""))
    finally:
        rmtree_force(base)
    results.append(("e2e cleanup leaves no folder", not os.path.exists(base), base))
    return results
