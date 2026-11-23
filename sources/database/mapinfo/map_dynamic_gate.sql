CREATE TABLE "map_dynamic_gate" (
  map_name TEXT PRIMARY KEY,
  gate_type INTEGER NOT NULL,
  tile_x INTEGER NOT NULL,
  tile_y INTEGER NOT NULL,
  tile_w INTEGER NOT NULL,
  tile_h INTEGER NOT NULL,
  dest_map TEXT NOT NULL CHECK(length(dest_map) <= 10),
  dest_x INTEGER NOT NULL,
  dest_y INTEGER NOT NULL,
  FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
);
