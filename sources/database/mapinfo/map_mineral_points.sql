CREATE TABLE map_mineral_points (
        map_name TEXT NOT NULL,
        point_index INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, point_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_mineral_map ON map_mineral_points(map_name);
