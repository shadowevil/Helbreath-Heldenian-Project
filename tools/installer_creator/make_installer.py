#!/usr/bin/env python3
"""Create native installers from a game directory.

Generates:
  - Windows: Inno Setup .iss script → compile with iscc to produce a .exe installer
  - Linux:   Self-extracting .sh script (bash + embedded tar.gz)

Settings are saved to installer.json — delete it to reconfigure.
"""

import json
import os
import io
import sys
import shutil
import stat
import tarfile
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(SCRIPT_DIR, "installer.json")

SKIP_DIRS = {"cache", "logs", "save", "updates", ".git", "__pycache__", "build"}
SKIP_EXTENSIONS = {".old", ".update", ".log", ".bak", ".pdb", ".ilk", ".exp"}

DEFAULTS = {
    "source_dir": ".",
    "game_title": "Helbreath",
    "output_dir": SCRIPT_DIR,
    "platforms": "3",
    "exe_name": "Game_x64_win.exe",
}


def resolve_path(path: str) -> str:
    """Resolve a (possibly relative) path to absolute, based on SCRIPT_DIR."""
    path = path.strip("\"'")
    if os.path.isabs(path):
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(SCRIPT_DIR, path))


def make_relative(path: str) -> str:
    """Convert an absolute path to relative (from SCRIPT_DIR) if possible."""
    try:
        rel = os.path.relpath(path, SCRIPT_DIR)
        # On Windows, relpath across drives raises or returns an absolute path
        if os.path.isabs(rel):
            return path
        return rel
    except ValueError:
        return path


def prompt(text: str, default: str = "") -> str:
    if default:
        result = input(f"{text} [{default}]: ").strip()
        return result if result else default
    return input(f"{text}: ").strip()


def format_size(size_bytes: int) -> str:
    if size_bytes >= 1 << 30:
        return f"{size_bytes / (1 << 30):.1f} GB"
    if size_bytes >= 1 << 20:
        return f"{size_bytes / (1 << 20):.1f} MB"
    if size_bytes >= 1 << 10:
        return f"{size_bytes / (1 << 10):.1f} KB"
    return f"{size_bytes} B"


def collect_files(root_dir: str) -> list[str]:
    files = []
    root_dir = os.path.normpath(root_dir)

    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames[:] = [
            d for d in dirnames
            if d.lower() not in SKIP_DIRS and not d.startswith(".")
        ]

        for filename in sorted(filenames):
            _, ext = os.path.splitext(filename)
            if ext.lower() in SKIP_EXTENSIONS:
                continue
            if ".bak_" in filename:
                continue
            files.append(os.path.join(dirpath, filename))

    return files


def is_executable(rel_path: str) -> bool:
    _, ext = os.path.splitext(rel_path)
    if ext.lower() in (".exe", ".dll", ".so"):
        return True
    if not ext:
        return True
    return False


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

def load_config() -> dict | None:
    if not os.path.isfile(CONFIG_PATH):
        return None
    try:
        with open(CONFIG_PATH) as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None


def save_config(cfg: dict):
    with open(CONFIG_PATH, "w", newline="\n") as f:
        json.dump(cfg, f, indent=4)
    print(f"  Config saved to {CONFIG_PATH}\n")


def run_setup() -> dict:
    """Interactive setup. Returns config dict."""

    # Source directory
    source_dir = prompt("Directory to pack", DEFAULTS["source_dir"])
    while True:
        resolved = resolve_path(source_dir)
        if not os.path.isdir(resolved):
            print(f"  '{resolved}' is not a valid directory.")
            source_dir = prompt("Directory to pack")
            continue

        print(f"  Resolved: {resolved}")
        ok = prompt("  Correct? (y/n)", "y")
        if ok.lower() in ("y", "yes"):
            source_dir = resolved
            break
        source_dir = prompt("Directory to pack")

    # Game title
    dir_name = os.path.basename(source_dir)
    game_title = prompt("Game title", dir_name if dir_name != "." else DEFAULTS["game_title"])

    # Output directory
    output_dir = prompt("Output directory", SCRIPT_DIR)
    output_dir = resolve_path(output_dir)

    # Platform selection
    print("\nPlatforms:")
    print("  1. Windows (.exe via Inno Setup)")
    print("  2. Linux (self-extracting .sh)")
    print("  3. Both")
    platforms = prompt("Which platform(s)?", "3")

    # Windows exe name
    exe_name = DEFAULTS["exe_name"]
    if platforms in ("1", "3"):
        exe_candidates = [
            f for f in os.listdir(source_dir)
            if f.endswith(".exe") and "Game" in f
        ]
        default_exe = exe_candidates[0] if exe_candidates else DEFAULTS["exe_name"]
        exe_name = prompt("Windows executable name", default_exe)

    cfg = {
        "source_dir": make_relative(source_dir),
        "game_title": game_title,
        "output_dir": make_relative(output_dir),
        "platforms": platforms,
        "exe_name": exe_name,
    }
    save_config(cfg)
    return cfg


def print_config(cfg: dict):
    print(f"  Source:    {resolve_path(cfg.get('source_dir', DEFAULTS['source_dir']))}")
    print(f"  Title:     {cfg.get('game_title', DEFAULTS['game_title'])}")
    print(f"  Output:    {resolve_path(cfg.get('output_dir', DEFAULTS['output_dir']))}")
    platforms = cfg.get("platforms", "3")
    plat_str = {"1": "Windows", "2": "Linux", "3": "Both"}.get(platforms, platforms)
    print(f"  Platforms: {plat_str}")
    if platforms in ("1", "3"):
        print(f"  Exe:       {cfg.get('exe_name', DEFAULTS['exe_name'])}")


# ---------------------------------------------------------------------------
# Windows — Inno Setup .iss generation
# ---------------------------------------------------------------------------

def generate_inno_setup(source_dir: str, game_title: str, version: str,
                        exe_name: str, output_dir: str) -> str:
    """Generate an Inno Setup .iss script file."""

    safe_title = game_title.replace(" ", "")
    iss_name = f"{safe_title}_setup.iss"
    iss_path = os.path.join(output_dir, iss_name)

    # Use forward slashes in source for the ISS SourceDir
    source_dir_iss = source_dir.replace("/", "\\")

    iss_content = f"""; Inno Setup script for {game_title}
; Generated by make_installer.py

[Setup]
AppName={game_title}
AppVersion={version}
AppPublisher=Helbreath
DefaultDirName={{autopf}}\\{game_title}
DefaultGroupName={game_title}
OutputDir={output_dir.replace('/', chr(92))}
OutputBaseFilename={safe_title}_v{version}_setup
Compression=lzma2
SolidCompression=yes
SetupIconFile=compiler:SetupClassicIcon.ico
UninstallDisplayIcon={{app}}\\{exe_name}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
DisableWelcomePage=no
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{{app}}"; Permissions: users-modify

[Files]
Source: "{source_dir_iss}\\*"; DestDir: "{{app}}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{{group}}\\{game_title}"; Filename: "{{app}}\\{exe_name}"
Name: "{{autodesktop}}\\{game_title}"; Filename: "{{app}}\\{exe_name}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{{app}}\\{exe_name}"; Description: "Launch {game_title}"; Flags: nowait postinstall skipifsilent
"""

    with open(iss_path, "w", newline="\r\n") as f:
        f.write(iss_content)

    return iss_path


def compile_inno_setup(iss_path: str) -> bool:
    """Try to compile the .iss with iscc. Returns True on success."""

    # Common install locations for Inno Setup
    iscc_candidates = [
        "iscc",  # on PATH
        r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        r"C:\Program Files\Inno Setup 6\ISCC.exe",
        r"C:\Program Files (x86)\Inno Setup 5\ISCC.exe",
    ]

    iscc_path = None
    for candidate in iscc_candidates:
        if shutil.which(candidate) or os.path.isfile(candidate):
            iscc_path = candidate
            break

    if not iscc_path:
        return False

    print(f"  Compiling with: {iscc_path}")
    result = subprocess.run([iscc_path, iss_path], capture_output=True, text=True)

    if result.returncode == 0:
        print("  Compilation succeeded!")
        # Find output exe name from stdout
        for line in result.stdout.splitlines():
            if "Output setup file" in line or ".exe" in line.lower():
                print(f"  {line.strip()}")
        return True
    else:
        print(f"  Compilation failed (exit code {result.returncode})")
        if result.stderr:
            for line in result.stderr.strip().splitlines()[:5]:
                print(f"    {line}")
        return False


# ---------------------------------------------------------------------------
# Linux — self-extracting .sh generation
# ---------------------------------------------------------------------------

LINUX_SH_HEADER = r'''#!/bin/bash
# Self-extracting installer for {game_title} v{version}
# Generated by make_installer.py

set -e

GAME_TITLE="{game_title}"
VERSION="{version}"
ARCHIVE_LINE={archive_line}

echo "=== $GAME_TITLE Installer (v$VERSION) ==="
echo ""

# Default install directory
DEFAULT_DIR="$HOME/Games/$GAME_TITLE"

# Prompt for install directory
read -rp "Install directory [$DEFAULT_DIR]: " INSTALL_DIR
INSTALL_DIR="${{INSTALL_DIR:-$DEFAULT_DIR}}"

# Expand ~ if present
INSTALL_DIR="${{INSTALL_DIR/#\~/$HOME}}"

# Resolve to absolute path
INSTALL_DIR="$(cd "$(dirname "$INSTALL_DIR")" 2>/dev/null && pwd)/$(basename "$INSTALL_DIR")" 2>/dev/null || INSTALL_DIR="$(readlink -f "$INSTALL_DIR" 2>/dev/null || echo "$INSTALL_DIR")"

echo "  Target: $INSTALL_DIR"

if [ -d "$INSTALL_DIR" ] && [ "$(ls -A "$INSTALL_DIR" 2>/dev/null)" ]; then
    read -rp "  Directory exists and is not empty. Overwrite? (y/n) [n]: " OVERWRITE
    if [ "${{OVERWRITE,,}}" != "y" ] && [ "${{OVERWRITE,,}}" != "yes" ]; then
        echo "  Cancelled."
        exit 0
    fi
fi

read -rp "  Proceed with installation? (y/n) [y]: " CONFIRM
CONFIRM="${{CONFIRM:-y}}"
if [ "${{CONFIRM,,}}" != "y" ] && [ "${{CONFIRM,,}}" != "yes" ]; then
    echo "  Cancelled."
    exit 0
fi

echo ""
echo "Extracting..."

mkdir -p "$INSTALL_DIR"

# Extract the embedded tar.gz payload
tail -n +$ARCHIVE_LINE "$0" | tar xz -C "$INSTALL_DIR"

# Set executable permissions on binaries
find "$INSTALL_DIR" -maxdepth 1 -type f ! -name "*.*" -exec chmod +x {{}} \;
find "$INSTALL_DIR" -maxdepth 1 -name "*.sh" -exec chmod +x {{}} \;

FILE_COUNT=$(find "$INSTALL_DIR" -type f | wc -l)
DIR_SIZE=$(du -sh "$INSTALL_DIR" | cut -f1)

echo "  Installed $FILE_COUNT files ($DIR_SIZE) to $INSTALL_DIR"

# Desktop shortcut
read -rp "Create desktop shortcut? (y/n) [y]: " SHORTCUT
SHORTCUT="${{SHORTCUT:-y}}"
if [ "${{SHORTCUT,,}}" = "y" ] || [ "${{SHORTCUT,,}}" = "yes" ]; then
    DESKTOP_DIR="${{XDG_DESKTOP_DIR:-$HOME/Desktop}}"
    if [ -d "$DESKTOP_DIR" ]; then
        # Find the game executable (extensionless binary)
        EXE_PATH=$(find "$INSTALL_DIR" -maxdepth 1 -name "Game_*_linux" -type f | head -1)
        if [ -z "$EXE_PATH" ]; then
            EXE_PATH=$(find "$INSTALL_DIR" -maxdepth 1 -type f ! -name "*.*" | head -1)
        fi

        if [ -n "$EXE_PATH" ]; then
            DESKTOP_FILE="$DESKTOP_DIR/$(echo "$GAME_TITLE" | tr ' ' '-' | tr '[:upper:]' '[:lower:]').desktop"
            cat > "$DESKTOP_FILE" << DESKTOP_EOF
[Desktop Entry]
Name=$GAME_TITLE
Type=Application
Exec=$EXE_PATH
Path=$INSTALL_DIR
Terminal=false
Categories=Game;
DESKTOP_EOF
            chmod +x "$DESKTOP_FILE"
            echo "  Shortcut created: $DESKTOP_FILE"
        else
            echo "  No game executable found, skipping shortcut."
        fi
    else
        echo "  Desktop directory not found, skipping shortcut."
    fi
fi

echo ""
echo "Installation complete!"
exit 0

'''


def generate_linux_installer(source_dir: str, game_title: str, version: str,
                             output_dir: str) -> str:
    """Generate a self-extracting .sh installer with embedded tar.gz."""

    safe_title = game_title.lower().replace(" ", "_")
    sh_name = f"{safe_title}_v{version}_linux_install.sh"
    sh_path = os.path.join(output_dir, sh_name)

    files = collect_files(source_dir)
    total = len(files)
    total_bytes = sum(os.path.getsize(f) for f in files)

    print(f"  {total} files, {format_size(total_bytes)} total")
    print(f"  Creating tar.gz payload...")

    # Create tar.gz in memory
    tar_buffer = io.BytesIO()
    packed_bytes = 0
    with tarfile.open(fileobj=tar_buffer, mode="w:gz", compresslevel=6) as tf:
        for i, full_path in enumerate(files):
            rel_path = os.path.relpath(full_path, source_dir).replace("\\", "/")
            file_size = os.path.getsize(full_path)

            pct = (packed_bytes / total_bytes * 100) if total_bytes else 0
            print(f"\r  [{pct:5.1f}%] ({i + 1}/{total}) {rel_path[:60]:<60}",
                  end="", flush=True)

            tf.add(full_path, arcname=rel_path)
            packed_bytes += file_size

    print(f"\r  [100.0%] Done.{' ' * 60}")

    tar_data = tar_buffer.getvalue()
    print(f"  Payload size: {format_size(len(tar_data))}")

    # Build the shell script header
    # We need to know what line the archive starts at
    # First, generate header with a placeholder, count lines, then regenerate
    header_text = LINUX_SH_HEADER.format(
        game_title=game_title,
        version=version,
        archive_line="PLACEHOLDER",
    )
    # Count lines in header (the archive starts on the next line after header)
    header_lines = header_text.count("\n") + 1

    # Regenerate with correct line number
    header_text = LINUX_SH_HEADER.format(
        game_title=game_title,
        version=version,
        archive_line=header_lines,
    )

    # Write script: header (text) + payload (binary)
    with open(sh_path, "wb") as f:
        f.write(header_text.encode("utf-8"))
        f.write(tar_data)

    # Make executable
    try:
        st = os.stat(sh_path)
        os.chmod(sh_path, st.st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)
    except OSError:
        pass  # Windows — chmod not meaningful

    return sh_path


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("=== Helbreath Installer Creator ===")
    print(f"  Working from: {SCRIPT_DIR}\n")

    # Load or create config
    cfg = load_config()
    if cfg is None:
        print("  No config found — running first-time setup.")
        print(f"  (Delete {CONFIG_PATH} to reconfigure)\n")
        cfg = run_setup()
    else:
        print(f"  Loaded config from {CONFIG_PATH}")
        print_config(cfg)
        print()
        reconfig = prompt("Update config?", "n")
        if reconfig.lower() in ("y", "yes"):
            cfg = run_setup()

    source_dir = cfg.get("source_dir", DEFAULTS["source_dir"])
    game_title = cfg.get("game_title", DEFAULTS["game_title"])
    output_dir = cfg.get("output_dir", DEFAULTS["output_dir"])
    platforms = cfg.get("platforms", "3")
    exe_name = cfg.get("exe_name", DEFAULTS["exe_name"])

    # Resolve paths
    source_dir = resolve_path(source_dir)
    output_dir = resolve_path(output_dir)

    if not os.path.isdir(source_dir):
        print(f"  Error: '{source_dir}' is not a valid directory.")
        rerun = prompt("  Run setup to reconfigure?", "y")
        if rerun.lower() in ("y", "yes"):
            cfg = run_setup()
            source_dir = resolve_path(cfg.get("source_dir", DEFAULTS["source_dir"]))
            output_dir = resolve_path(cfg.get("output_dir", DEFAULTS["output_dir"]))
            game_title = cfg.get("game_title", DEFAULTS["game_title"])
            platforms = cfg.get("platforms", "3")
            exe_name = cfg.get("exe_name", DEFAULTS["exe_name"])
            if not os.path.isdir(source_dir):
                print(f"  Error: '{source_dir}' is still not a valid directory.")
                return 1
        else:
            return 1

    os.makedirs(output_dir, exist_ok=True)

    # Always ask for version and platform
    version = prompt("Version", "0.2.0")
    while True:
        parts = version.split(".")
        if len(parts) == 3 and all(p.isdigit() for p in parts):
            break
        print("  Invalid format. Use MAJOR.MINOR.PATCH (e.g. 0.2.0)")
        version = prompt("Version", "0.2.0")

    print("\nPlatforms:")
    print("  1. Windows (.exe via Inno Setup)")
    print("  2. Linux (self-extracting .sh)")
    print("  3. Both")
    platforms = prompt("Which platform(s)?", platforms)

    do_windows = platforms in ("1", "3")
    do_linux = platforms in ("2", "3")

    print("")

    # Generate Windows installer
    if do_windows:
        print("--- Windows Installer (Inno Setup) ---")
        iss_path = generate_inno_setup(
            source_dir, game_title, version, exe_name, output_dir
        )
        print(f"  Wrote: {iss_path}")

        compile_now = prompt("\n  Compile .iss to .exe now?", "y")
        if compile_now.lower() in ("y", "yes"):
            if not compile_inno_setup(iss_path):
                print("  Inno Setup (iscc) not found.")
                print("  Install from: https://jrsoftware.org/isdl.php")
                print(f"  Then run: iscc \"{iss_path}\"")
        else:
            print(f"  To compile later: iscc \"{iss_path}\"")
        print("")

    # Generate Linux installer
    if do_linux:
        print("--- Linux Installer (self-extracting .sh) ---")
        sh_path = generate_linux_installer(
            source_dir, game_title, version, output_dir
        )
        total_size = format_size(os.path.getsize(sh_path))
        print(f"  Wrote: {sh_path} ({total_size})")
        print(f"  Users run: chmod +x {os.path.basename(sh_path)} && ./{os.path.basename(sh_path)}")
        print("")

    print("Done!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
