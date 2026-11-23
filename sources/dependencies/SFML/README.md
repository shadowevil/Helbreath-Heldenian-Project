# SFML 3.0 Setup

This directory should contain SFML 3.0 headers and libraries for the SFMLEngine to compile.

## Installation Options

### Option 1: Download SFML 3.0 (Recommended)

1. Download SFML 3.0 from https://www.sfml-dev.org/download.php
2. Choose the Visual C++ version matching your compiler (VS 2022 = VC17)
3. Choose the 32-bit (x86) version
4. Extract to this directory:
   - `include/SFML/*.hpp` - Headers
   - `lib/*.lib` - Libraries

### Option 2: Use vcpkg

```bash
vcpkg install sfml:x86-windows
```

Then copy the files to this directory or update the vcxproj include/library paths.

### Option 3: Build from Source

```bash
git clone https://github.com/SFML/SFML.git
cd SFML
cmake -B build -DCMAKE_INSTALL_PREFIX=path/to/Dependencies/SFML -A Win32
cmake --build build --config Release
cmake --install build --config Release
```

## Required Libraries

For Release builds, link against:
- sfml-graphics.lib
- sfml-window.lib
- sfml-system.lib

For Debug builds, link against:
- sfml-graphics-d.lib
- sfml-window-d.lib
- sfml-system-d.lib

## Directory Structure

After setup, this directory should look like:

```
Dependencies/SFML/
├── include/
│   └── SFML/
│       ├── Graphics.hpp
│       ├── Graphics/
│       │   ├── RenderWindow.hpp
│       │   ├── RenderTexture.hpp
│       │   ├── Texture.hpp
│       │   ├── Image.hpp
│       │   ├── Sprite.hpp
│       │   ├── Font.hpp
│       │   ├── Text.hpp
│       │   └── ...
│       ├── Window.hpp
│       ├── Window/
│       │   ├── Event.hpp
│       │   └── ...
│       └── System.hpp
└── lib/
    ├── sfml-graphics.lib
    ├── sfml-window.lib
    ├── sfml-system.lib
    ├── sfml-graphics-d.lib (Debug)
    ├── sfml-window-d.lib (Debug)
    └── sfml-system-d.lib (Debug)
```

## Client Configuration

To build the game with SFML instead of DirectDraw:

1. In Visual Studio, go to Client project properties
2. Under Linker > Input > Additional Dependencies:
   - Remove: `DDrawEngine.lib`
   - Add: `SFMLEngine.lib`, `sfml-graphics.lib`, `sfml-window.lib`, `sfml-system.lib`
3. Or create a new build configuration (Debug-SFML, Release-SFML) with these settings

## Notes

- SFML 3.0 requires C++17 or later
- The SFMLEngine uses SFML_STATIC define for static linking
- Make sure to copy SFML DLLs to output directory if using dynamic linking
