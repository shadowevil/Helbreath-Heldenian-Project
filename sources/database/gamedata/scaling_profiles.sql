CREATE TABLE scaling_profiles (
            profile_id  TEXT NOT NULL,
            bracket_min INTEGER NOT NULL,
            bracket_max INTEGER NOT NULL,
            multiplier  REAL NOT NULL DEFAULT 1.0,
            PRIMARY KEY (profile_id, bracket_min)
        );
