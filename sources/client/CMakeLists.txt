cmake_minimum_required(VERSION 3.16)

# Versioning: the release comes from shared/version.h, the Build ID from build_tools/gen_build_id.py
include(${CMAKE_CURRENT_SOURCE_DIR}/../build_tools/versioning.cmake)
hb_read_release(client HB_CLIENT_VERSION)

project(HelbreathClient
    VERSION ${HB_CLIENT_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# SFMLEngine static library (9 .cpp files)
# ---------------------------------------------------------------------------
set(SFMLENGINE_SOURCES
    ../sfml_engine/renderer_factory.cpp
    ../sfml_engine/sfml_bitmap_font.cpp
    ../sfml_engine/sfml_input.cpp
    ../sfml_engine/sfml_renderer.cpp
    ../sfml_engine/sfml_sprite.cpp
    ../sfml_engine/sfml_sprite_factory.cpp
    ../sfml_engine/sfml_text_renderer.cpp
    ../sfml_engine/sfml_texture.cpp
    ../sfml_engine/sfml_window.cpp
)

# ---------------------------------------------------------------------------
# CControls static library (UI controls — no game/engine dependencies)
# ---------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ccontrols ${CMAKE_CURRENT_BINARY_DIR}/CControls_build)

add_library(SFMLEngine STATIC ${SFMLENGINE_SOURCES})

# ---------------------------------------------------------------------------
# AutoUpdater static library
# ---------------------------------------------------------------------------
set(AUTOUPDATER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../auto_updater)
set(AUTOUPDATER_SOURCES
    ${AUTOUPDATER_DIR}/auto_updater.cpp
    ${AUTOUPDATER_DIR}/updater_manifest.cpp
    ${AUTOUPDATER_DIR}/updater_http.cpp
    ${AUTOUPDATER_DIR}/updater_sha256.cpp
    ${AUTOUPDATER_DIR}/updater_file_ops.cpp
    ${AUTOUPDATER_DIR}/updater_gui_x11.cpp
)

add_library(AutoUpdater STATIC ${AUTOUPDATER_SOURCES})

target_include_directories(AutoUpdater PRIVATE
    ${AUTOUPDATER_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared
)

find_package(X11 REQUIRED)
target_link_libraries(AutoUpdater PRIVATE X11::X11)

# ---------------------------------------------------------------------------
# Shared sources (8 .cpp files)
# ---------------------------------------------------------------------------
set(SHARED_SOURCES
    ../dependencies/shared/log/log_backend.cpp
    ../dependencies/shared/net/asio_socket.cpp
    ../dependencies/shared/net/io_service_pool.cpp
    ../dependencies/shared/render/application.cpp
    ../dependencies/shared/render/event.cpp
    ../dependencies/shared/render/sprite_factory.cpp
    ../dependencies/shared/render/resolution_config.cpp
    ../dependencies/shared/render/text_lib.cpp
    ../dependencies/shared/util/sha256.cpp
    ../dependencies/shared/util/hmac.cpp
    ../dependencies/shared/version.cpp
    ../dependencies/shared/game/formula_engine.cpp
)

# ---------------------------------------------------------------------------
# Client sources (all .cpp in sources/client/)
# ---------------------------------------------------------------------------
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS "*.cpp") # re-globbed each build, so a new .cpp is picked up

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(HelbreathClient
    ${CLIENT_SOURCES}
    ${SHARED_SOURCES}
)

# Output name: Game_x64_linux / Game_x86_linux
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_arch "x64")
else()
    set(_arch "x86")
endif()
set_target_properties(HelbreathClient PROPERTIES OUTPUT_NAME "Game_${_arch}_linux")
# Output: <repo>/bin/client/<config>/Game_<arch>_linux (build_tools/layout.cmake)
hb_set_output(HelbreathClient client)

# ---------------------------------------------------------------------------
# Include directories (mirrors vcxproj IncludePath)
# ---------------------------------------------------------------------------
set(SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared)

set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccontrols
    ${CMAKE_CURRENT_SOURCE_DIR}/../sfml_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../auto_updater
    ${SHARED_DIR}
    ${SHARED_DIR}/entity
    ${SHARED_DIR}/game
    ${SHARED_DIR}/net
    ${SHARED_DIR}/render
    ${SHARED_DIR}/util
    ${SHARED_DIR}/log
    ${SHARED_DIR}/ASIO
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/client/includes
)

target_include_directories(SFMLEngine PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(HelbreathClient PRIVATE ${COMMON_INCLUDE_DIRS})

# Build ID header (CMake binary dir) and release seed include dirs, regenerated before every build
hb_add_versioning(HelbreathClient)

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
target_compile_definitions(SFMLEngine PRIVATE SFML_STATIC SFML_ENGINE)
target_compile_definitions(HelbreathClient PRIVATE SFML_STATIC SFML_ENGINE)
target_compile_definitions(HelbreathClient PRIVATE TESTER_ONLY)
# _DEBUG in Debug, as the Windows projects define it (Client.vcxproj, SFMLEngine.vcxproj); GCC does not
# add it. The debug-only client code and DEF_CLAUDE_TESTING key off it (d-027, HH-0036).
target_compile_definitions(SFMLEngine PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
target_compile_definitions(HelbreathClient PRIVATE $<$<CONFIG:Debug>:_DEBUG>)

# ---------------------------------------------------------------------------
# Find SFML 3 — try system install first, fall back to FetchContent
# ---------------------------------------------------------------------------
find_package(SFML 3 COMPONENTS Graphics Window System QUIET)

if(NOT SFML_FOUND)
    message(STATUS "SFML 3 not found on system — fetching from GitHub (3.0.2)...")
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        3.0.2
        GIT_SHALLOW    TRUE
    )
    # Only build the modules we need
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SFML)
endif()

# SFMLEngine needs SFML headers
target_link_libraries(SFMLEngine PRIVATE SFML::Graphics SFML::Window SFML::System)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

target_link_libraries(HelbreathClient PRIVATE
    CControls
    AutoUpdater
    SFMLEngine
    SFML::Graphics
    SFML::Window
    SFML::System
    Threads::Threads
    X11::X11
    ${CMAKE_DL_LIBS}
)
