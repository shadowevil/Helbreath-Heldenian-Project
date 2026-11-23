cmake_minimum_required(VERSION 3.16)

# Versioning: the release comes from shared/version.h, the Build ID from build_tools/gen_build_id.py
include(${CMAKE_CURRENT_SOURCE_DIR}/../build_tools/versioning.cmake)
hb_read_release(server HB_SERVER_VERSION)

project(HelbreathServer
    VERSION ${HB_SERVER_VERSION}
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Server sources (51 .cpp files)
# ---------------------------------------------------------------------------
set(SERVER_SOURCES
    account_sqlite_store.cpp
    client.cpp
    cmd_broadcast.cpp
    cmd_give_item.cpp
    cmd_help.cpp
    cmd_reload.cpp
    cmd_set_admin.cpp
    cmd_save_all.cpp
    cmd_set_cmd_level.cpp
    cmd_shutdown.cpp
    cmd_show_chat.cpp
    combat_manager.cpp
    common_types.cpp
    crafting_manager.cpp
    database_gate.cpp
    regen_manager.cpp
    delay_event_manager.cpp
    dynamic_object_manager.cpp
    entity_manager.cpp
    fishing_manager.cpp
    game.cpp
    game_chat_command.cpp
    game_cmd_block.cpp
    game_cmd_come.cpp
    game_cmd_create_item.cpp
    game_cmd_give_item.cpp
    game_cmd_gm.cpp
    game_cmd_goto.cpp
    game_cmd_invis.cpp
    game_cmd_regen.cpp
    game_cmd_save_all.cpp
    game_cmd_spawn.cpp
    game_cmd_unblock.cpp
    game_cmd_whisper.cpp
    game_config_sqlite_store.cpp
    server_config.cpp
    guild_manager.cpp
    item_manager.cpp
    login_server.cpp
    loot_manager.cpp
    magic_manager.cpp
    map.cpp
    map_info_sqlite_store.cpp
    mining_manager.cpp
    network_msg.cpp
    npc.cpp
    party_manager.cpp
    quest.cpp
    quest_manager.cpp
    server_command.cpp
    server_console.cpp
    server_log.cpp
    skill_manager.cpp
    status_effect_manager.cpp
    tile.cpp
    war_manager.cpp
    wmain.cpp
)

# ---------------------------------------------------------------------------
# Shared sources (3 .cpp files)
# ---------------------------------------------------------------------------
set(SHARED_SOURCES
    ../dependencies/shared/net/asio_socket.cpp
    ../dependencies/shared/net/io_service_pool.cpp
    ../dependencies/shared/log/log_backend.cpp
    ../dependencies/shared/util/sha256.cpp
    ../dependencies/shared/util/hmac.cpp
    ../dependencies/shared/version.cpp
    ../dependencies/shared/game/formula_engine.cpp
)

# ---------------------------------------------------------------------------
# SQLite amalgamation (compiled as C)
# ---------------------------------------------------------------------------
set(SQLITE_SOURCE
    ../dependencies/server/includes/SQLite/sqlite3.c
)
set_source_files_properties(${SQLITE_SOURCE} PROPERTIES LANGUAGE C)
set_source_files_properties(${SQLITE_SOURCE} PROPERTIES COMPILE_FLAGS "-w")

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(HelbreathServer
    ${SERVER_SOURCES}
    ${SHARED_SOURCES}
    ${SQLITE_SOURCE}
)
# Output: <repo>/bin/server/<config>/HelbreathServer (build_tools/layout.cmake)
hb_set_output(HelbreathServer server)

# ---------------------------------------------------------------------------
# Include directories (mirrors vcxproj IncludePath)
# ---------------------------------------------------------------------------
target_include_directories(HelbreathServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/ASIO
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/entity
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/game
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/net
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/render
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/util
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/shared/log
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/server/includes
)

# Build ID header (CMake binary dir) and release seed include dirs, regenerated before every build
hb_add_versioning(HelbreathServer)

# Schema headers from sources/database (gamedata/mapinfo fingerprints, account baseline + migrations), written
# before every build into the same per-channel folder hb_add_versioning puts on the include path. Header
# only: no DB is built or touched. Rewritten only when the content changes.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
string(TOLOWER ${HB_ACTIVE_ENVIRONMENT} HB_SERVER_CHANNEL_DIR)
set(HB_SERVER_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/${HB_SERVER_CHANNEL_DIR})
add_custom_target(HelbreathServer_schema ALL
    COMMAND ${Python3_EXECUTABLE} ${HB_BUILD_TOOLS_DIR}/gen_server_schema.py
        --environment ${HB_ACTIVE_ENVIRONMENT}
        --schema-out ${HB_SERVER_GENERATED_DIR}/schema_sql_generated.h
        --account-out ${HB_SERVER_GENERATED_DIR}/account_schema_generated.h
    BYPRODUCTS ${HB_SERVER_GENERATED_DIR}/schema_sql_generated.h ${HB_SERVER_GENERATED_DIR}/account_schema_generated.h
    COMMENT "Writing the schema headers for HelbreathServer"
    VERBATIM)
add_dependencies(HelbreathServer HelbreathServer_schema)

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
target_compile_definitions(HelbreathServer PRIVATE TESTER_ONLY)

# ---------------------------------------------------------------------------
# Linux link libraries
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(HelbreathServer PRIVATE
    Threads::Threads
    ${CMAKE_DL_LIBS}
)
