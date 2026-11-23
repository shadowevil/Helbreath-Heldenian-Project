CREATE TABLE map_energy_sphere_creation (
        map_name TEXT NOT NULL,
        point_index INTEGER NOT NULL,
        sphere_type INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, point_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_sphere_creation_map ON map_energy_sphere_creation(map_name);
