# Build layout for the Linux (CMake) builds - the CMake twin of layout.props (Phase H: sources/ holds
# sources only). Included by versioning.cmake, so every CMakeLists.txt that uses versioning gets it.
#
#   HB_REPO_ROOT    repo root
#   HB_BUILD_ROOT   <repo>/build   - CMake build dirs (build/linux/<project>), generated headers, release seed
#   HB_BIN_ROOT     <repo>/bin     - built executables, bin/<project>/<config>/
#
#   hb_set_output(HelbreathServer server)   # after add_executable(): output to bin/server/<config>/
#
# The build_*_linux.sh scripts use build/linux/<project> as the build directory; game/ is assembled from
# bin/ by sources/build_tools/deploy.py, never by CMake.
include_guard(GLOBAL)

get_filename_component(HB_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(HB_BUILD_ROOT "${HB_REPO_ROOT}/build")
set(HB_BIN_ROOT "${HB_REPO_ROOT}/bin")

# $<CONFIG> is CMAKE_BUILD_TYPE for the single-config Makefile generator, and stops a multi-config
# generator from appending its own per-config folder.
function(hb_set_output target project_key)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${HB_BIN_ROOT}/${project_key}/$<CONFIG>")
endfunction()
