CREATE TABLE "map_npc_avoid_rects" (
  map_name TEXT NOT NULL,
  rect_index INTEGER NOT NULL CHECK(rect_index >= 0 AND rect_index < 50),
  tile_x INTEGER NOT NULL,
  tile_y INTEGER NOT NULL,
  tile_w INTEGER NOT NULL,
  tile_h INTEGER NOT NULL,
  PRIMARY KEY (map_name, rect_index),
  FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
);

CREATE INDEX idx_npc_avoid_map ON map_npc_avoid_rects(map_name);
