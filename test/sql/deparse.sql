\c single :ROLE_SUPERUSER
CREATE SCHEMA IF NOT EXISTS "customSchema" AUTHORIZATION :ROLE_DEFAULT_PERM_USER;
\c single :ROLE_DEFAULT_PERM_USER

CREATE TABLE test1 (time timestamp, temp float8);
CREATE TABLE test2 (time timestamp NOT NULL, temp float8);
-- dimension sizes still not handled TODO::
CREATE TABLE test3 (time timestamp NOT NULL, temp float8 NOT NULL, temp2 int[], temp3 int[][], temp4 int [][][], temp5 int[4][4]);
CREATE TABLE test4 (time timestamp, device INT PRIMARY KEY, temp float8);
CREATE TABLE test5 (time timestamp, name CHAR(30) NOT NULL, vname VARCHAR );

CREATE TABLE "customSchema".test6 (time timestamp, temp float8);


-- \c single :ROLE_SUPERUSER
-- CREATE OR REPLACE FUNCTION _timescaledb_internal.deparse_test(tablename TEXT)
-- RETURNS TEXT
-- AS '@MODULE_PATHNAME@', 'deparse_test'
-- LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
-- \c single :ROLE_DEFAULT_PERM_USER

SELECT _timescaledb_internal.deparse_test('public.test1');
SELECT _timescaledb_internal.deparse_test('public.test2');
SELECT _timescaledb_internal.deparse_test('public.test3');
SELECT _timescaledb_internal.deparse_test('public.test4'); -- Q:: TODO how to get primary key info? is this in constraints or in pg_type?
SELECT _timescaledb_internal.deparse_test('public.test5');

SELECT _timescaledb_internal.deparse_test('customSchema.test6'); -- this fails for some reason TODO
