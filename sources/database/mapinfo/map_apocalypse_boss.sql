CREATE TABLE "map_apocalypse_boss" (
  map_name TEXT PRIMARY KEY,
  npc_id INTEGER NOT NULL,
  tile_x INTEGER NOT NULL,
  tile_y INTEGER NOT NULL,
  tile_w INTEGER NOT NULL,
  tile_h INTEGER NOT NULL,
  FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
);
