CREATE TABLE "map_npcs" (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    map_name TEXT NOT NULL,
    npc_config_id INTEGER NOT NULL,
    move_type INTEGER NOT NULL,
    waypoint_list TEXT NOT NULL DEFAULT '',
    name_prefix TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
);

CREATE INDEX idx_npcs_map ON map_npcs(map_name);
