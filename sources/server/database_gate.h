#pragma once

#include <string_view>

struct sqlite3;

// The refuse-to-start gate for the generated databases (gamedata.db, mapinfo.db).
//
// sources/database/build_databases.py generates them from the SQL source directory and stamps their meta
// table with schema_fingerprint, sql_release and channel. The server never creates or alters their tables:
// it opens an existing file and checks those three keys against this binary (the fingerprint the build
// embedded in schema_sql_generated.h, shared/version.h's sql_release and the build channel).
namespace hb::server::database_gate
{
	// Opens file_name (relative to the working directory) read-write without creating it and verifies its
	// meta keys. On any failure logs one error saying what differs and what to run, and returns nullptr.
	[[nodiscard]] sqlite3* open_generated(std::string_view file_name, std::string_view expected_fingerprint);

} // namespace hb::server::database_gate
