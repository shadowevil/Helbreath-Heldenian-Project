CREATE TABLE map_energy_sphere_goal (
        map_name TEXT NOT NULL,
        point_index INTEGER NOT NULL,
        result INTEGER NOT NULL,
        aresden_x INTEGER NOT NULL,
        aresden_y INTEGER NOT NULL,
        elvine_x INTEGER NOT NULL,
        elvine_y INTEGER NOT NULL,
        PRIMARY KEY (map_name, point_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_sphere_goal_map ON map_energy_sphere_goal(map_name);
