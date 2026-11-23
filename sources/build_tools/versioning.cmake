# Versioning for the Linux (CMake) builds, shared by client/ and server/CMakeLists.txt.
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../build_tools/versioning.cmake)
#   hb_read_release(client HB_CLIENT_VERSION)       # before project(): "0.2.52" from shared/version.h
#   hb_add_versioning(HelbreathClient)              # after add_executable()
#
# Per-configure settings (-D...). They are deliberately NOT sticky: each is read once and then removed from
# the cache, so a TEST or PROD configure never silently carries over into the next one. Pass them on every
# configure that needs them; any re-configure without them (including an automatic one) is DEV.
#
# Linux TEST/PROD build: from a fresh clone at the approved commit, from the repo root, with a fresh build
# directory (the same one the build_*_linux.sh scripts use), run
#   cmake -S sources/<Client|Server> -B build/linux/<client|server> -DCMAKE_BUILD_TYPE=Release \
#         -DHB_ENVIRONMENT=TEST -DHB_FRESH_SOURCE=ON
#   cmake --build build/linux/<client|server>
# and do not edit CMakeLists.txt, versioning.cmake or version.h before make finishes: any of those makes
# make re-configure without the settings, which turns the build into DEV (configure warns when it does).
# The build_*_linux.sh scripts do not do TEST/PROD yet.
#   HB_ENVIRONMENT     DEV (default), TEST or PROD
#   HB_FRESH_SOURCE    ON asserts the source was freshly obtained from Git (TEST/PROD need it)
#
# Sticky setting:
#   HB_RELEASE_SEED_DIR  base folder of the per-channel seed, <dir>/<channel>/release_seed_generated.h;
#                        the client and the server must share it. Default <repo>/build/generated
#                        (git-ignored), the same files the Windows build uses, so a seed rolled by
#                        build_release.ps1 is picked up by a WSL build of the same checkout.

include(${CMAKE_CURRENT_LIST_DIR}/layout.cmake)

set(HB_BUILD_TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR})
set(HB_VERSION_HEADER ${CMAKE_CURRENT_LIST_DIR}/../dependencies/shared/version.h)

set(HB_ACTIVE_ENVIRONMENT DEV)
if(DEFINED HB_ENVIRONMENT)
    set(HB_ACTIVE_ENVIRONMENT ${HB_ENVIRONMENT})
endif()
unset(HB_ENVIRONMENT CACHE)
if(NOT HB_ACTIVE_ENVIRONMENT MATCHES "^(DEV|TEST|PROD)$")
    message(FATAL_ERROR "HB_ENVIRONMENT must be DEV, TEST or PROD, not '${HB_ACTIVE_ENVIRONMENT}'")
endif()
# Fails safe (the binary is labelled DEV), but say so loudly: an automatic re-configure during `make` (an
# edited CMakeLists.txt, versioning.cmake or version.h) drops a TEST/PROD configure back to DEV.
if(DEFINED HB_LAST_ENVIRONMENT AND NOT HB_LAST_ENVIRONMENT STREQUAL "DEV" AND HB_ACTIVE_ENVIRONMENT STREQUAL "DEV")
    message(WARNING "Versioning: this build directory was configured for ${HB_LAST_ENVIRONMENT} and is now DEV, "
        "because HB_ENVIRONMENT is not sticky. The binaries built from here are DEV builds. For a "
        "${HB_LAST_ENVIRONMENT} build, re-run cmake with -DHB_ENVIRONMENT=${HB_LAST_ENVIRONMENT} -DHB_FRESH_SOURCE=ON.")
endif()
set(HB_LAST_ENVIRONMENT ${HB_ACTIVE_ENVIRONMENT} CACHE INTERNAL "The environment of the last configure")

set(HB_ACTIVE_FRESH_SOURCE OFF)
if(DEFINED HB_FRESH_SOURCE AND HB_FRESH_SOURCE)
    set(HB_ACTIVE_FRESH_SOURCE ON)
endif()
unset(HB_FRESH_SOURCE CACHE)
message(STATUS "Versioning: environment ${HB_ACTIVE_ENVIRONMENT}, fresh source ${HB_ACTIVE_FRESH_SOURCE}")

set(HB_RELEASE_SEED_DIR ${HB_BUILD_ROOT}/generated CACHE PATH
    "Base folder of the per-channel release seed, shared by the client and the server")

# Reads `inline constexpr release <component>_release{M, m, p, maturity::x};` out of version.h, the single
# source of truth, into <out_var> as "M.m.p". Re-runs configure when version.h changes.
function(hb_read_release component out_var)
    file(STRINGS ${HB_VERSION_HEADER} _line REGEX "inline constexpr release ${component}_release[{]")
    string(REGEX MATCH "[{] *([0-9]+) *, *([0-9]+) *, *([0-9]+) *," _match "${_line}")
    if(NOT _match)
        message(FATAL_ERROR "could not read ${component}_release from ${HB_VERSION_HEADER}")
    endif()
    set(${out_var} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HB_VERSION_HEADER})
endfunction()

# Derives the Build ID before every build of <target> (rewritten only when it changes, so an unchanged
# source state does not recompile), ensures the channel's release seed for non-Debug builds, and adds the
# generated include directories.
function(hb_add_versioning target)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    # The Build ID header does not depend on the configuration, so one folder serves every configuration
    # of a multi-config generator; the configuration only feeds the environment gates and the seed step.
    # It does depend on the channel: one folder per channel, so a DEV header can never feed a TEST build.
    string(TOLOWER ${HB_ACTIVE_ENVIRONMENT} _channel_dir)
    set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated/${_channel_dir})
    set(_build_id_header ${_generated_dir}/build_id_generated.h)
    set(_seed_dir ${HB_RELEASE_SEED_DIR}/${_channel_dir})

    get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_multi_config)
        set(_configuration $<CONFIG>)
    elseif(CMAKE_BUILD_TYPE)
        set(_configuration ${CMAKE_BUILD_TYPE})
    else()
        # matches the CMakeLists.txt default
        set(_configuration Release)
    endif()

    set(_fresh_switch)
    if(HB_ACTIVE_FRESH_SOURCE)
        set(_fresh_switch --fresh-source)
    endif()

    add_custom_target(${target}_version ALL
        COMMAND ${Python3_EXECUTABLE} ${HB_BUILD_TOOLS_DIR}/gen_build_id.py
            --environment ${HB_ACTIVE_ENVIRONMENT} --configuration ${_configuration}
            --output ${_build_id_header} ${_fresh_switch}
        # a no-op for Debug, which needs no seed
        COMMAND ${Python3_EXECUTABLE} ${HB_BUILD_TOOLS_DIR}/gen_release_seed.py
            --mode ensure --environment ${HB_ACTIVE_ENVIRONMENT} --configuration ${_configuration}
            --output ${_seed_dir}/release_seed_generated.h
        BYPRODUCTS ${_build_id_header}
        COMMENT "Deriving the Build ID for ${target}"
        VERBATIM)
    add_dependencies(${target} ${target}_version)
    target_include_directories(${target} PRIVATE ${_generated_dir} ${_seed_dir})
endfunction()
