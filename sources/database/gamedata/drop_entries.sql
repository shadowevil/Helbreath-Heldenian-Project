CREATE TABLE drop_entries (
  drop_table_id INTEGER NOT NULL,
  tier INTEGER NOT NULL,
  item_id INTEGER NOT NULL,
  weight INTEGER NOT NULL,
  min_count INTEGER NOT NULL,
  max_count INTEGER NOT NULL,
  PRIMARY KEY (drop_table_id, tier, item_id)
);
