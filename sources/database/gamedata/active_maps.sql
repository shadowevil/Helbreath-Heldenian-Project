CREATE TABLE active_maps (
            map_index INTEGER PRIMARY KEY,
            map_name TEXT NOT NULL,
            active INTEGER NOT NULL DEFAULT 0
        );
