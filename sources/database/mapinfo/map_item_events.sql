CREATE TABLE map_item_events (
        map_name TEXT NOT NULL,
        event_index INTEGER NOT NULL,
        item_name TEXT NOT NULL CHECK(length(item_name) <= 20),
        amount INTEGER NOT NULL,
        total_num INTEGER NOT NULL,
        event_month INTEGER NOT NULL,
        event_day INTEGER NOT NULL,
        event_type INTEGER NOT NULL DEFAULT 0,
        mob_list TEXT NOT NULL DEFAULT '',
        PRIMARY KEY (map_name, event_index),
        FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE
    );

CREATE INDEX idx_item_events_map ON map_item_events(map_name);
