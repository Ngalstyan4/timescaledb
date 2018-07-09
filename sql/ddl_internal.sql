CREATE OR REPLACE FUNCTION _timescaledb_internal.dimension_get_time(
    hypertable_id INT
)
    RETURNS _timescaledb_catalog.dimension LANGUAGE SQL STABLE AS
$BODY$
    SELECT *
    FROM _timescaledb_catalog.dimension d
    WHERE d.hypertable_id = dimension_get_time.hypertable_id AND
          d.interval_length IS NOT NULL
$BODY$;

-- Show chunks older than the given timestamp.
CREATE OR REPLACE FUNCTION _timescaledb_internal.show_chunks_impl(
    hypertable_name REGCLASS,
    older_than_time BIGINT = NULL,
    newer_than_time BIGINT = NULL
)
    RETURNS SETOF REGCLASS LANGUAGE PLPGSQL VOLATILE AS
$BODY$
DECLARE
    chunk_row _timescaledb_catalog.chunk;
    exist_count INT = 0;
    _table_name TEXT;
    _schema_name TEXT;
BEGIN

    IF older_than_time IS NULL AND newer_than_time IS NULL THEN
        RAISE 'Cannot have both arguments to show_chunks be NULL';
    END IF;
    SELECT c.relname, n.nspname
    FROM  pg_class c
            LEFT JOIN pg_namespace n
            ON n.oid = c.relnamespace
    WHERE c.oid=hypertable_name::regclass
    INTO _table_name, _schema_name;

    IF hypertable_name IS NOT NULL THEN
        SELECT COUNT(*)
        FROM _timescaledb_catalog.hypertable h
        WHERE (_schema_name IS NULL OR h.schema_name = _schema_name)
        AND _table_name = h.table_name
        INTO STRICT exist_count;

        IF exist_count = 0 THEN
            RAISE 'hypertable % does not exist', _table_name -- Q:: should this be REGCLASS full name of the tabke instead?
            USING ERRCODE = 'IO001';
        END IF;
    END IF;

    RETURN QUERY
    SELECT CONCAT(c.schema_name, '.',c.table_name)::REGCLASS AS chunk -- Q:: is casting typesafe? Q:: AS part does not seem to do anything, why?
        FROM _timescaledb_catalog.chunk c
        INNER JOIN _timescaledb_catalog.hypertable h ON (h.id = c.hypertable_id)
        INNER JOIN _timescaledb_internal.dimension_get_time(h.id) time_dimension ON(true)
        INNER JOIN _timescaledb_catalog.dimension_slice ds
            ON (ds.dimension_id = time_dimension.id)
        INNER JOIN _timescaledb_catalog.chunk_constraint cc
            ON (cc.dimension_slice_id = ds.id AND cc.chunk_id = c.id)
        WHERE (older_than_time IS NULL OR ds.range_end <= older_than_time)
        AND (newer_than_time IS NULL OR ds.range_start >= newer_than_time)
        AND (_schema_name IS NULL OR h.schema_name = _schema_name)
        AND (_table_name IS NULL OR h.table_name = _table_name);
END
$BODY$;

-- Drop chunks older than the given timestamp. If a hypertable name is given,
-- drop only chunks associated with this table. Any of the first three arguments
-- can be NULL meaning "all values".
CREATE OR REPLACE FUNCTION _timescaledb_internal.drop_chunks_impl(
    older_than_time  BIGINT,
    table_name  NAME = NULL,
    schema_name NAME = NULL,
    cascade  BOOLEAN = FALSE,
    truncate_before  BOOLEAN = FALSE,
    newer_than_time BIGINT = NULL
)
    RETURNS VOID LANGUAGE PLPGSQL VOLATILE AS
$BODY$
DECLARE
    chunk_row REGCLASS;
    cascade_mod TEXT = '';
    exist_count INT = 0;
BEGIN
    IF older_than_time IS NULL AND newer_than_time IS NULL AND table_name IS NULL AND schema_name IS NULL THEN
        RAISE 'Cannot have older_than_time, newer_than_time and all table identifiers to drop_chunks be NULL';
    END IF;

    IF cascade THEN
        cascade_mod = 'CASCADE';
    END IF;

    IF table_name IS NOT NULL THEN
        SELECT COUNT(*)
        FROM _timescaledb_catalog.hypertable h
        WHERE (drop_chunks_impl.schema_name IS NULL OR h.schema_name = drop_chunks_impl.schema_name)
        AND drop_chunks_impl.table_name = h.table_name
        INTO STRICT exist_count;

        IF exist_count = 0 THEN
            RAISE 'hypertable % does not exist', drop_chunks_impl.table_name
            USING ERRCODE = 'IO001';
        END IF;
    END IF;

    FOR chunk_row IN SELECT _timescaledb_internal.show_chunks_impl(table_name::REGCLASS, older_than_time, newer_than_time)
    LOOP
        IF truncate_before THEN
            EXECUTE format(
                $$
                TRUNCATE %s %s
                $$, chunk_row, cascade_mod
            );
        END IF;
        -- Q:: Using %s for correct indentaiton of regclass,
        -- see https://dba.stackexchange.com/questions/141113/pl-pgsql-regclass-quoting-of-table-named-like-keyword
        EXECUTE format(
                $$
                DROP TABLE %s %s
                $$, chunk_row, cascade_mod
        );
    END LOOP;
END
$BODY$;
CREATE OR REPLACE FUNCTION _timescaledb_internal.time_dim_type_check(
    given_type REGTYPE,
    table_name  NAME,
    schema_name NAME
)
    RETURNS VOID LANGUAGE PLPGSQL STABLE AS
$BODY$
DECLARE
    actual_type regtype;
BEGIN
    BEGIN
        WITH hypertable_ids AS (
            SELECT id
            FROM _timescaledb_catalog.hypertable h
            WHERE (time_dim_type_check.schema_name IS NULL OR h.schema_name = time_dim_type_check.schema_name) AND
            (time_dim_type_check.table_name IS NULL OR h.table_name = time_dim_type_check.table_name)
        )
        SELECT DISTINCT time_dim.column_type INTO STRICT actual_type
        FROM hypertable_ids INNER JOIN LATERAL _timescaledb_internal.dimension_get_time(hypertable_ids.id) time_dim ON (true);
    EXCEPTION
        WHEN NO_DATA_FOUND THEN
            RAISE EXCEPTION 'No hypertables found';
        WHEN TOO_MANY_ROWS THEN
            RAISE EXCEPTION 'Cannot use drop_chunks on multiple tables with different time types';
    END;

    IF given_type IN ('int'::regtype, 'smallint'::regtype, 'bigint'::regtype ) THEN
        IF actual_type IN ('int'::regtype, 'smallint'::regtype, 'bigint'::regtype ) THEN
            RETURN;
        END IF;
    END IF;
    IF actual_type != given_type THEN
        RAISE EXCEPTION 'Cannot call drop_chunks with a % on hypertables with a time type of: %', given_type, actual_type;
    END IF;
END
$BODY$;

-- TODO rename function
-- type chcker for show_chunks new type approach;
CREATE OR REPLACE FUNCTION _timescaledb_internal.time_dim_type_check(
    given_type REGTYPE,
    hypertable_name REGCLASS
)
    RETURNS VOID LANGUAGE PLPGSQL STABLE AS
$BODY$
DECLARE
    actual_type REGTYPE;
    _table_name TEXT;
    _schema_name TEXT;
BEGIN
    SELECT c.relname, n.nspname
        FROM  pg_class c
                LEFT JOIN pg_namespace n
                ON n.oid = c.relnamespace
    WHERE c.oid=hypertable_name::REGCLASS
    INTO _table_name, _schema_name;
    PERFORM _timescaledb_internal.time_dim_type_check(given_type, _table_name, _schema_name);
END
$BODY$;

--documentation of these function located in chunk_index.h
CREATE OR REPLACE FUNCTION _timescaledb_internal.chunk_index_clone(chunk_index_oid OID) RETURNS OID
AS '@MODULE_PATHNAME@', 'chunk_index_clone' LANGUAGE C VOLATILE STRICT;

CREATE OR REPLACE FUNCTION _timescaledb_internal.chunk_index_replace(chunk_index_oid_old OID, chunk_index_oid_new OID) RETURNS VOID
AS '@MODULE_PATHNAME@', 'chunk_index_replace' LANGUAGE C VOLATILE STRICT;
