CREATE TABLE summon_thresholds (
    threshold_id INTEGER PRIMARY KEY AUTOINCREMENT,
    min_mastery INTEGER NOT NULL,
    npc_id INTEGER NOT NULL,
    weight INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY (npc_id) REFERENCES "npc_configs_old"(npc_id)
);
