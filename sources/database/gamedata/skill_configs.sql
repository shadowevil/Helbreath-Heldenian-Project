CREATE TABLE "skill_configs" (
  "skill_id" INTEGER,
  "name" TEXT NOT NULL,
  "skill_type" INTEGER NOT NULL,
  "value1" INTEGER NOT NULL,
  "value2" INTEGER NOT NULL,
  "value3" INTEGER NOT NULL,
  "value4" INTEGER NOT NULL,
  "value5" INTEGER NOT NULL,
  "value6" INTEGER NOT NULL, is_useable INTEGER NOT NULL DEFAULT 0, use_method INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY ("skill_id")
);
