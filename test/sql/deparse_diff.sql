SET client_min_messages = 'fatal';
\set ECHO none
\o /dev/null

-- turn off alignment to avoid + signs at the end of lines
\a
-- turn off headers
\t
\ir include/deparse_create.sql
\a
\t

\o :TEST_OUTPUT_DIR/results/deparse_create.out
\d+ (public|"customSchema").*

\a
\t
\o /dev/null
\ir include/deparse_recreate.sql
\a
\t

\o :TEST_OUTPUT_DIR/results/deparse_recreate.out
\d+ (public|"customSchema").*

\o
\set ECHO 'all'
\! diff -y --suppress-common-lines  -W 240 ${TEST_OUTPUT_DIR}/results/deparse_create.out ${TEST_OUTPUT_DIR}/results/deparse_recreate.out