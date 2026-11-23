# Code search (grep.py)

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 1 KB. One tool, one usage block; nothing to split. Moved out of CLAUDE.md 2026-11-23.*

`.claude/tools/grep.py` searches C++ sources only; use the Grep tool for anything else. Two modes: brief (`-b`) for quick stdout, detailed for a full-context log file.

```
python .claude/tools/grep.py "pattern" -b                       # brief: one line per match to stdout
python .claude/tools/grep.py "pattern"                          # detailed: context blocks to log file
python .claude/tools/grep.py "pattern" -F --path sources/client # fixed string, scoped to directory
python .claude/tools/grep.py "pattern" -C 8 -i -o results.log  # 8 context lines, case-insensitive
```

Brief (`-b`) prints `file:line | match` to stdout. Detailed (default) writes to `.claude/scratch/tools/grep/grep_results.log` with context, enclosing scope and `>>>` markers. The output dir auto-clears at 10 MB.
