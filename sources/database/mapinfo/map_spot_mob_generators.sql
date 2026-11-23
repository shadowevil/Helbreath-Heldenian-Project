CREATE TABLE "map_spot_mob_generators" (
    map_name TEXT NOT NULL,
    generator_index INTEGER NOT NULL CHECK(generator_index >= 0 AND generator_index < 100),
    generator_type INTEGER NOT NULL CHECK(generator_type IN (1, 2)),
    tile_x INTEGER NOT NULL DEFAULT 0,
    tile_y INTEGER NOT NULL DEFAULT 0,
    tile_w INTEGER NOT NULL DEFAULT 0,
    tile_h INTEGER NOT NULL DEFAULT 0,
    waypoints TEXT NOT NULL DEFAULT '',
    npc_config_id INTEGER NOT NULL,
    max_mobs INTEGER NOT NULL,
    prob_sa INTEGER NOT NULL DEFAULT 15,
    kind_sa INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (map_name, generator_index),
    FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
);

CREATE INDEX idx_spot_mob_map ON map_spot_mob_generators(map_name);
