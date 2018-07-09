

\ir include/deparse_load.sql
\o :TEST_OUTPUT_DIR/results/deparse_reference.out
\d+ (public|"customSchema").*
\o
-- turn off alignment to avoid + signs at the end of lines
\a
-- tunr off headers
\t
SELECT format('DROP TABLE public.test1; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test1');
\gexec
SELECT format('DROP TABLE public.test2; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test2');
\gexec
SELECT format('DROP TABLE public.test3; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test3');
\gexec
SELECT format('DROP TABLE public.test4; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test4');
\gexec
SELECT format('DROP TABLE public.test5; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test5');
\gexec
SELECT format('DROP TABLE public.test6; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test6');
\gexec
SELECT format('DROP TABLE public.test7; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test7');
\gexec
SELECT format('DROP TABLE public.test8; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test8');
\gexec
SELECT format('DROP TABLE public.test9; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test9');
\gexec
SELECT format('DROP TABLE public.test10; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test10');
\gexec

SELECT format('DROP TABLE public.test12; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test12');
\gexec
SELECT format('DROP TABLE public.test13; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test13');
\gexec
SELECT format('DROP TABLE public.test11 CASCADE; %s', deparse_test) FROM _timescaledb_internal.deparse_test('public.test11');
\gexec
DROP TABLE test12, test13;
CREATE TABLE test12 (
    time TIMESTAMP REFERENCES test11(time_start),
    blah TIMESTAMP REFERENCES test11(time_start)
);
CREATE TABLE test13 (time TIMESTAMP, FOREIGN KEY (time) REFERENCES test11(time_start));

SELECT format('DROP TABLE "customSchema".test14; %s', deparse_test) FROM _timescaledb_internal.deparse_test('"customSchema".test14');
\gexec
SELECT format('DROP TABLE "customSchema"."ha ha"; %s', deparse_test) FROM _timescaledb_internal.deparse_test('"customSchema"."ha ha"');
\gexec


\a
\t
\o :TEST_OUTPUT_DIR/results/deparse_test.out
\d+ (public|"customSchema").*
\o

\! diff ${TEST_OUTPUT_DIR}/results/deparse_reference.out ${TEST_OUTPUT_DIR}/results/deparse_test.out