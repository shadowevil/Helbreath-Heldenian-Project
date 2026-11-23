CREATE TABLE map_strike_points (
        map_name TEXT NOT NULL,
        point_index INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        hp INTEGER NOT NULL,
        effect_x1 INTEGER NOT NULL,
        effect_y1 INTEGER NOT NULL,
        effect_x2 INTEGER NOT NULL,
        effect_y2 INTEGER NOT NULL,
        effect_x3 INTEGER NOT NULL,
        effect_y3 INTEGER NOT NULL,
        effect_x4 INTEGER NOT NULL,
        effect_y4 INTEGER NOT NULL,
        effect_x5 INTEGER NOT NULL,
        effect_y5 INTEGER NOT NULL,
        related_map_name TEXT NOT NULL CHECK(length(related_map_name) <= 10),
        PRIMARY KEY (map_name, point_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_strike_map ON map_strike_points(map_name);
