\c single :ROLE_SUPERUSER
CREATE SCHEMA IF NOT EXISTS "customSchema" AUTHORIZATION :ROLE_DEFAULT_PERM_USER;
\c single :ROLE_DEFAULT_PERM_USER

CREATE TABLE test1 (time timestamp, temp float8);
SELECT _timescaledb_internal.deparse_test('public.test1');

CREATE TABLE test2 (time timestamp NOT NULL, temp float8);
SELECT _timescaledb_internal.deparse_test('public.test2');

-- dimension sizes still not handled TODO::
CREATE TABLE test3 (time timestamp NOT NULL, temp float8 NOT NULL, temp2 int[], temp3 int[][], temp4 int [][][], temp5 int[4][4]);
SELECT _timescaledb_internal.deparse_test('public.test3');

CREATE TABLE test4 (time timestamp, device INT PRIMARY KEY, temp float8);
SELECT _timescaledb_internal.deparse_test('public.test4'); -- Q:: TODO how to get primary key info? is this in constraints or in pg_type?

CREATE TABLE test5 (time timestamp, name CHAR(30) NOT NULL, vname VARCHAR );
SELECT _timescaledb_internal.deparse_test('public.test5');

CREATE TABLE test6 (time TIMESTAMP NOT NULL,
      num INT NOT NULL DEFAULT 48,
      n INT DEFAULT 455454,
      tt TEXT,
      tt_col TEXT COLLATE "es_ES",
      tt_def_col TEXT DEFAULT ('ա' COLLATE "hy_AM")
);
SELECT _timescaledb_internal.deparse_test('public.test6');

CREATE TABLE test7 (time_start TIMESTAMP NOT NULL,
                    time_end   TIMESTAMP NOT NULL CHECK (time_end >= time_start),
                    val_min    INT CHECK (val_min < val_max),
                    val_max    INT CHECK (val_max > val_min)
);
SELECT _timescaledb_internal.deparse_test('public.test7');
CREATE TABLE test8 (time_start TIMESTAMP NOT NULL,
                    time_end   TIMESTAMP NOT NULL CHECK (time_end >= time_start),
                    val_min    INT CONSTRAINT min_less_max CHECK (val_min < val_max),
                    val_max    INT CHECK (val_min < val_max) -- Q:: can I reuse constraints by name?
);
ALTER TABLE public.test8 ADD PRIMARY KEY (time_start);
ALTER TABLE public.test8 ADD UNIQUE (time_end);
SELECT _timescaledb_internal.deparse_test('public.test8');

CREATE TABLE test9 (time_start TIMESTAMP NOT NULL,
                    val_min    INT,
                    val_max    INT
);
ALTER TABLE public.test9 ADD CONSTRAINT cnstr CHECK (val_min < val_max);
SELECT _timescaledb_internal.deparse_test('public.test9');

CREATE TABLE test10 (time_start TIMESTAMP NOT NULL,
                    "strange name"    TEXT CHECK ("strange name"  != 'specific case here') DEFAULT 'multiword name',
                    "Странное название"    INT
);
SELECT _timescaledb_internal.deparse_test('public.test10');

-- errors
\set ON_ERROR_STOP 0
SELECT _timescaledb_internal.deparse_test('nonexistent');

CREATE TEMP TABLE test10 (time_start TIMESTAMP NOT NULL);
SELECT _timescaledb_internal.deparse_test('test10'); -- not schema qualified

CREATE UNLOGGED TABLE test11 (time_start TIMESTAMP NOT NULL);
SELECT _timescaledb_internal.deparse_test('public.test11');

CREATE TABLE test12 (time_start TIMESTAMP PRIMARY KEY DEFERRABLE INITIALLY IMMEDIATE);
SELECT _timescaledb_internal.deparse_test('public.test12');

CREATE TABLE test13 (time_start TIMESTAMP PRIMARY KEY DEFERRABLE INITIALLY DEFERRED);
SELECT _timescaledb_internal.deparse_test('public.test13');

\set ON_ERROR_STOP 1


CREATE TABLE test14 (time_start TIMESTAMP NOT NULL PRIMARY KEY);
SELECT _timescaledb_internal.deparse_test('public.test14');

CREATE TABLE test15 (
    time TIMESTAMP REFERENCES test14(time_start),
    blah TIMESTAMP REFERENCES test14(time_start)
);
SELECT _timescaledb_internal.deparse_test('public.test15');

CREATE TABLE test16 (time TIMESTAMP, FOREIGN KEY (time) REFERENCES test14(time_start));
SELECT _timescaledb_internal.deparse_test('public.test16');


CREATE TABLE "customSchema".test17 (time timestamp, temp float8);

SELECT _timescaledb_internal.deparse_test('customSchema.test17'); -- this fails for some reason TODO Q::
