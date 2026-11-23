CREATE TABLE map_initial_points (
        map_name TEXT NOT NULL,
        point_index INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, point_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_initial_points_map ON map_initial_points(map_name);
