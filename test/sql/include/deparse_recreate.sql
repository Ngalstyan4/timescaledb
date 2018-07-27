\c single :ROLE_SUPERUSER
CREATE OR REPLACE FUNCTION test.deparse(tablename REGCLASS)
RETURNS TEXT
AS :MODULE_PATHNAME, 'deparse_create_table_test'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
\c single :ROLE_DEFAULT_PERM_USER
SET client_min_messages = 'fatal';


SELECT format('DROP TABLE public.test1; %s', deparse) FROM test.deparse('public.test1');
\gexec
SELECT format('DROP TABLE public.test2; %s ;-- ss', deparse) FROM test.deparse('public.test2');
\gexec
SELECT format('DROP TABLE public.test3; %s', deparse) FROM test.deparse('public.test3');
\gexec
SELECT format('DROP TABLE public.test4; %s', deparse) FROM test.deparse('public.test4');
\gexec
SELECT format('DROP TABLE public.test5; %s', deparse) FROM test.deparse('public.test5');
\gexec
SELECT format('DROP TABLE public.test6; %s', deparse) FROM test.deparse('public.test6');
\gexec
SELECT format('DROP TABLE public.test7; %s', deparse) FROM test.deparse('public.test7');
\gexec
SELECT format('DROP TABLE public.test8; %s', deparse) FROM test.deparse('public.test8');
\gexec
SELECT format('DROP TABLE public.test9; %s', deparse) FROM test.deparse('public.test9');
\gexec
SELECT format('DROP TABLE public.test10; %s', deparse) FROM test.deparse('public.test10');
\gexec

SELECT format('DROP TABLE public.test11 CASCADE; %s', deparse) FROM test.deparse('public.test11');
\gexec

-- tables 12 and 13 depend on table 11. when dropping table 11, some references form
-- tables 12 and 13 are dropped. The following two commands make sure that
-- tables 12 and 13 have the ame schema as they did before so we compare the same things
-- these are not directly testing anything.
DROP TABLE test12, test13;
CREATE TABLE test12 (
    time TIMESTAMP REFERENCES test11(time_start),
    blah TIMESTAMP REFERENCES test11(time_start)
);
CREATE TABLE test13 (time TIMESTAMP, FOREIGN KEY (time) REFERENCES test11(time_start));

SELECT format('DROP TABLE public.test12; %s;', deparse) FROM test.deparse('public.test12');
\gexec
SELECT format('DROP TABLE public.test13; %s', deparse) FROM test.deparse('public.test13');
\gexec
SELECT format('DROP TABLE "customSchema".test14; %s', deparse) FROM test.deparse('"customSchema".test14');
\gexec
SELECT format('DROP TABLE "customSchema"."ha ha"; %s', deparse) FROM test.deparse('"customSchema"."ha ha"');
\gexec