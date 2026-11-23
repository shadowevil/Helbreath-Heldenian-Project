CREATE TABLE map_heldenian_towers (
        map_name TEXT NOT NULL,
        tower_index INTEGER NOT NULL,
        type_id INTEGER NOT NULL,
        side INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, tower_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_heldenian_towers_map ON map_heldenian_towers(map_name);
