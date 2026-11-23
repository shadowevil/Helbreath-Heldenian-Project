CREATE TABLE map_waypoints (
        map_name TEXT NOT NULL,
        waypoint_index INTEGER NOT NULL,
        x INTEGER NOT NULL,
        y INTEGER NOT NULL,
        PRIMARY KEY (map_name, waypoint_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_waypoints_map ON map_waypoints(map_name);
