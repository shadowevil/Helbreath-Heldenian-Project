CREATE TABLE admins ( account_name TEXT PRIMARY KEY, character_name TEXT NOT NULL, approved_ip TEXT NOT NULL DEFAULT '0.0.0.0', admin_level INTEGER NOT NULL DEFAULT 1);
