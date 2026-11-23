# Building Helbreath on Linux

Command reference, output paths and the deploy layout live in the repository's `.claude/docs/build.md`. This file covers first-time setup (WSL, prerequisites) and notes.

## Building from Windows via WSL

If you are on Windows, install WSL2 and build from there. Your Windows drives are mounted under `/mnt/`.

1. Install WSL2 (from admin PowerShell):
   ```powershell
   wsl --install -d Ubuntu
   ```
   Reboot if prompted, then set up a username/password.

2. From inside WSL, go to the checkout:
   ```bash
   cd /mnt/c/path/to/Helbreath-Heldenian-Project   # adjust drive letter and path as needed
   ```
   Then follow the prerequisites and build steps below.

## Prerequisites

### Ubuntu / Debian

Server only:
```bash
sudo apt update && sudo apt install -y cmake g++ build-essential
```

Server and client:
```bash
sudo apt update && sudo apt install -y cmake g++ build-essential \
    libx11-dev libxi-dev libxrandr-dev libxcursor-dev \
    libgl1-mesa-dev libudev-dev libfreetype-dev
```

SFML 3 is fetched and built automatically by CMake FetchContent for the client. No manual SFML installation is needed.

## Building

Make the build scripts executable (only needed once):
```bash
chmod +x sources/build_server_linux.sh sources/build_client_linux.sh
```

### Server

```bash
./sources/build_server_linux.sh                 # Debug build (default)
./sources/build_server_linux.sh release         # Release build
./sources/build_server_linux.sh clean           # Delete build directory
./sources/build_server_linux.sh clean release   # Clean then rebuild release
./sources/build_server_linux.sh deploy-dry-run  # Build, then only preview the deploy (or: nodeploy)
```

### Client

```bash
./sources/build_client_linux.sh                 # Debug build (default)
./sources/build_client_linux.sh release         # Release build
./sources/build_client_linux.sh clean           # Delete build directory
./sources/build_client_linux.sh clean release   # Clean then rebuild release
./sources/build_client_linux.sh deploy-dry-run  # Build, then only preview the deploy (or: nodeploy)
```

### Output

| Config  | Server                               | Client                               |
|---------|--------------------------------------|--------------------------------------|
| Debug   | `bin/server/Debug/HelbreathServer`   | `bin/client/Debug/Game_x64_linux`    |
| Release | `bin/server/Release/HelbreathServer` | `bin/client/Release/Game_x64_linux`  |

Switching between Debug and Release without cleaning is fine: the scripts detect the change and reconfigure automatically.

## Deploying and running

After a successful build the script deploys into `game/server/` or `game/client/` (use `nodeploy` to skip). Do not copy binaries by hand. Run each from its deployed folder, where configs, data files and assets are:

```bash
cd game/server && ./HelbreathServer
cd game/client && ./Game_x64_linux
```

## Notes

- The server build uses `make -j2`. The client build uses `make -j$(nproc)` to use all available cores.
- CMake build files are stored in `build/linux/server/` and `build/linux/client/`. The `clean` command deletes these directories.
- If the build scripts have Windows line endings (CRLF) after transferring from Windows, convert them before running: `sed -i 's/\r$//' sources/build_server_linux.sh sources/build_client_linux.sh`
