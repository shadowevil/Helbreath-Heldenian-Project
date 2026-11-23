#!/bin/bash
# Linux build script for Helbreath Server
# Usage:
#   ./build_server_linux.sh                 Build (incremental, Debug)
#   ./build_server_linux.sh debug           Build Debug
#   ./build_server_linux.sh release         Build Release
#   ./build_server_linux.sh clean           Clean build directory and exit
#   ./build_server_linux.sh clean debug     Clean then rebuild Debug
#   ./build_server_linux.sh clean release   Clean then rebuild Release
#   ./build_server_linux.sh nodeploy        Build without deploying
#   ./build_server_linux.sh deploy-dry-run  Build, then only preview the deploy
#
# Layout (Phase H: sources/ holds sources only):
#   <repo>/build/linux/server/                  CMake build directory
#   <repo>/bin/server/<Debug|Release>/HelbreathServer
# After a successful build, sources/build_tools/deploy.py assembles <repo>/game/server (it never
# overwrites runtime state such as accounts/, logs or server_config.json).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/linux/server"
DO_CLEAN=false
BUILD_TYPE="Debug"
DO_DEPLOY=true
DEPLOY_DRY_RUN=""
# Keep the build tools from writing __pycache__ into sources/build_tools
export PYTHONDONTWRITEBYTECODE=1

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        clean)          DO_CLEAN=true ;;
        debug)          BUILD_TYPE="Debug" ;;
        release)        BUILD_TYPE="Release" ;;
        nodeploy)       DO_DEPLOY=false ;;
        deploy-dry-run) DEPLOY_DRY_RUN="--dry-run" ;;
        *)              echo "Unknown argument: $arg"; echo "Usage: $0 [clean] [debug|release] [nodeploy|deploy-dry-run]"; exit 1 ;;
    esac
done

# Clean
if [ "$DO_CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
    # If no build type was explicitly requested alongside clean, just exit
    if [ $# -eq 1 ]; then
        exit 0
    fi
fi

# Check if existing build was configured with a different build type
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_TYPE=$(grep -oP 'CMAKE_BUILD_TYPE:STRING=\K.*' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || true)
    if [ -n "$CACHED_TYPE" ] && [ "$CACHED_TYPE" != "$BUILD_TYPE" ]; then
        echo "Build type changed ($CACHED_TYPE -> $BUILD_TYPE), reconfiguring..."
        rm -rf "$BUILD_DIR"
    fi
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f CMakeCache.txt ]; then
    echo "Configuring (${BUILD_TYPE})..."
    cmake "$SCRIPT_DIR/server" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

echo "Building (${BUILD_TYPE})..."
make -j2

# CMake writes the binary straight to bin/ (build_tools/layout.cmake)
OUTPUT="$REPO_ROOT/bin/server/$BUILD_TYPE/HelbreathServer"
if [ ! -f "$OUTPUT" ]; then
    echo "ERROR: build finished but $OUTPUT is missing"
    exit 1
fi

echo ""
echo "Build complete: $OUTPUT"

if [ "$DO_DEPLOY" = true ]; then
    echo ""
    python3 "$SCRIPT_DIR/build_tools/deploy.py" --target server --config "$BUILD_TYPE" --platform linux $DEPLOY_DRY_RUN
fi
