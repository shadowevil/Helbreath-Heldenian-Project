CREATE TABLE event_schedule (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type TEXT NOT NULL,
            schedule_index INTEGER NOT NULL,
            day INTEGER NOT NULL,
            start_hour INTEGER NOT NULL,
            start_minute INTEGER NOT NULL,
            end_hour INTEGER,
            end_minute INTEGER,
            is_active INTEGER NOT NULL DEFAULT 0,
            UNIQUE(event_type, schedule_index)
        );
