CREATE TABLE map_teleport_locations (
        map_name TEXT NOT NULL,
        teleport_index INTEGER NOT NULL,
        src_x INTEGER NOT NULL,
        src_y INTEGER NOT NULL,
        dest_map_name TEXT NOT NULL CHECK(length(dest_map_name) <= 10),
        dest_x INTEGER NOT NULL,
        dest_y INTEGER NOT NULL,
        direction INTEGER NOT NULL CHECK(direction >= 0 AND direction <= 8),
        PRIMARY KEY (map_name, teleport_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_teleport_map ON map_teleport_locations(map_name);
