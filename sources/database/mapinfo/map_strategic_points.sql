CREATE TABLE map_strategic_points (
        map_name TEXT NOT NULL,
        point_index INTEGER NOT NULL,
        side INTEGER NOT NULL,
        point_value INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, point_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_strategic_map ON map_strategic_points(map_name);
