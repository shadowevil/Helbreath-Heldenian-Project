CREATE TABLE "map_no_attack_areas" (
  map_name TEXT NOT NULL,
  area_index INTEGER NOT NULL CHECK(area_index >= 0 AND area_index < 50),
  tile_x INTEGER NOT NULL,
  tile_y INTEGER NOT NULL,
  tile_w INTEGER NOT NULL,
  tile_h INTEGER NOT NULL,
  PRIMARY KEY (map_name, area_index),
  FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
);

CREATE INDEX idx_no_attack_map ON map_no_attack_areas(map_name);
