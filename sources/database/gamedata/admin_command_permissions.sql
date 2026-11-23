CREATE TABLE admin_command_permissions ( command TEXT PRIMARY KEY COLLATE NOCASE, admin_level INTEGER NOT NULL DEFAULT 1000, description TEXT NOT NULL DEFAULT '');
