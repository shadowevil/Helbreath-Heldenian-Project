CREATE TABLE map_heldenian_gate_doors (
        map_name TEXT NOT NULL,
        door_index INTEGER NOT NULL,
        direction INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, door_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_heldenian_doors_map ON map_heldenian_gate_doors(map_name);
