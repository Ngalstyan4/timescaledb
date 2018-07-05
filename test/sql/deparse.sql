\c single :ROLE_SUPERUSER
CREATE SCHEMA IF NOT EXISTS "customSchema" AUTHORIZATION :ROLE_DEFAULT_PERM_USER;
\c single :ROLE_DEFAULT_PERM_USER

CREATE TABLE test1 (time timestamp, temp float8);
CREATE TABLE test2 (time timestamp NOT NULL, temp float8);
-- dimension sizes still not handled TODO::
CREATE TABLE test3 (time timestamp NOT NULL, temp float8 NOT NULL, temp2 int[], temp3 int[][], temp4 int [][][], temp5 int[4][4]);
CREATE TABLE test4 (time timestamp, device INT PRIMARY KEY, temp float8);
CREATE TABLE test5 (time timestamp, name CHAR(30) NOT NULL, vname VARCHAR );
CREATE TABLE test6 (time TIMESTAMP NOT NULL,
      num INT NOT NULL DEFAULT 48,
      n INT DEFAULT 455454,
      tt TEXT,
      tt_col TEXT COLLATE "es_ES",
      tt_def_col TEXT DEFAULT ('ա' COLLATE "hy_AM")
);
CREATE TABLE test7 (time_start TIMESTAMP NOT NULL,
                    time_end   TIMESTAMP NOT NULL CHECK (time_end >= time_start),
                    val_min    INT CHECK (val_min < val_max),
                    val_max    INT CHECK (val_max > val_min)
);
-- CREATE TABLE "customSchema".test6 (time timestamp, temp float8);



SELECT _timescaledb_internal.deparse_test('public.test1');
SELECT _timescaledb_internal.deparse_test('public.test2');
SELECT _timescaledb_internal.deparse_test('public.test3');
SELECT _timescaledb_internal.deparse_test('public.test4'); -- Q:: TODO how to get primary key info? is this in constraints or in pg_type?
SELECT _timescaledb_internal.deparse_test('public.test5');
SELECT _timescaledb_internal.deparse_test('public.test6');
-- SELECT _timescaledb_internal.deparse_test('customSchema.test6'); -- this fails for some reason TODO
