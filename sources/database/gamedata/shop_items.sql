CREATE TABLE shop_items (
    shop_id INTEGER NOT NULL,
    item_id INTEGER NOT NULL, sort_order INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (shop_id, item_id)
);
