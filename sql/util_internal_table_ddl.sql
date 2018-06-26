-- This file contains functions associated with creating new
-- hypertables.

CREATE OR REPLACE FUNCTION _timescaledb_internal.dimension_is_finite(
    val      BIGINT
)
    RETURNS BOOLEAN LANGUAGE SQL IMMUTABLE PARALLEL SAFE AS
$BODY$
    --end values of bigint reserved for infinite
    SELECT val > (-9223372036854775808)::bigint AND val < 9223372036854775807::bigint
$BODY$;


CREATE OR REPLACE FUNCTION _timescaledb_internal.dimension_slice_get_constraint_sql(
    dimension_slice_id  INTEGER
)
    RETURNS TEXT LANGUAGE PLPGSQL VOLATILE AS
$BODY$
DECLARE
    dimension_slice_row _timescaledb_catalog.dimension_slice;
    dimension_row _timescaledb_catalog.dimension;
    parts TEXT[];
BEGIN
    SELECT * INTO STRICT dimension_slice_row
    FROM _timescaledb_catalog.dimension_slice
    WHERE id = dimension_slice_id;

    SELECT * INTO STRICT dimension_row
    FROM _timescaledb_catalog.dimension
    WHERE id = dimension_slice_row.dimension_id;

    IF dimension_row.partitioning_func IS NOT NULL THEN

        IF  _timescaledb_internal.dimension_is_finite(dimension_slice_row.range_start) THEN
            parts = parts || format(
            $$
                %1$I.%2$I(%3$I) >= %4$L
            $$,
            dimension_row.partitioning_func_schema,
            dimension_row.partitioning_func,
            dimension_row.column_name,
            dimension_slice_row.range_start);
        END IF;

        IF _timescaledb_internal.dimension_is_finite(dimension_slice_row.range_end) THEN
            parts = parts || format(
            $$
                %1$I.%2$I(%3$I) < %4$L
            $$,
            dimension_row.partitioning_func_schema,
            dimension_row.partitioning_func,
            dimension_row.column_name,
            dimension_slice_row.range_end);
        END IF;

        IF array_length(parts, 1) = 0 THEN
            RETURN NULL;
        END IF;
        return array_to_string(parts, 'AND');
    ELSE
        --TODO: only works with time for now
        IF _timescaledb_internal.time_literal_sql(dimension_slice_row.range_start, dimension_row.column_type) =
           _timescaledb_internal.time_literal_sql(dimension_slice_row.range_end, dimension_row.column_type) THEN
            RAISE 'Time based constraints have the same start and end values for column "%": %',
                    dimension_row.column_name,
                    _timescaledb_internal.time_literal_sql(dimension_slice_row.range_end, dimension_row.column_type);
        END IF;

        parts = ARRAY[]::text[];

        IF _timescaledb_internal.dimension_is_finite(dimension_slice_row.range_start) THEN
            parts = parts || format(
            $$
                 %1$I >= %2$s
            $$,
            dimension_row.column_name,
            _timescaledb_internal.time_literal_sql(dimension_slice_row.range_start, dimension_row.column_type));
        END IF;

        IF _timescaledb_internal.dimension_is_finite(dimension_slice_row.range_end) THEN
            parts = parts || format(
            $$
                 %1$I < %2$s
            $$,
            dimension_row.column_name,
            _timescaledb_internal.time_literal_sql(dimension_slice_row.range_end, dimension_row.column_type));
        END IF;

        return array_to_string(parts, 'AND');
    END IF;
END
$BODY$;

-- Outputs the create_hypertable command to recreate the given hypertable.
--
-- This is currently used internally for our single hypertable backup tool
-- so that it knows how to restore the hypertable without user intervention.
--
-- It only works for hypertables with up to 2 dimensions.
CREATE OR REPLACE FUNCTION _timescaledb_internal.get_create_command(
    table_name NAME
)
    RETURNS TEXT LANGUAGE PLPGSQL VOLATILE AS
$BODY$
DECLARE
    h_id             INTEGER;
    schema_name      NAME;
    time_column      NAME;
    time_interval    BIGINT;
    space_column     NAME;
    space_partitions INTEGER;
    dimension_cnt    INTEGER;
    dimension_row    record;
    ret              TEXT;
BEGIN
    SELECT h.id, h.schema_name
    FROM _timescaledb_catalog.hypertable AS h
    WHERE h.table_name = get_create_command.table_name
    INTO h_id, schema_name;

    IF h_id IS NULL THEN
        RAISE EXCEPTION 'hypertable % not found', table_name
        USING ERRCODE = 'IO101';
    END IF;

    SELECT COUNT(*)
    FROM _timescaledb_catalog.dimension d
    WHERE d.hypertable_id = h_id
    INTO STRICT dimension_cnt;

    IF dimension_cnt > 2 THEN
        RAISE EXCEPTION 'get_create_command only supports hypertables with up to 2 dimensions'
        USING ERRCODE = 'IO101';
    END IF;

    FOR dimension_row IN
        SELECT *
        FROM _timescaledb_catalog.dimension d
        WHERE d.hypertable_id = h_id
        LOOP
        IF dimension_row.interval_length IS NOT NULL THEN
            time_column := dimension_row.column_name;
            time_interval := dimension_row.interval_length;
        ELSIF dimension_row.num_slices IS NOT NULL THEN
            space_column := dimension_row.column_name;
            space_partitions := dimension_row.num_slices;
        END IF;
    END LOOP;

    ret := format($$SELECT create_hypertable('%I.%I', '%I'$$, schema_name, table_name, time_column);
    IF space_column IS NOT NULL THEN
        ret := ret || format($$, '%I', %s$$, space_column, space_partitions);
    END IF;
    ret := ret || format($$, chunk_time_interval => %s, create_default_indexes=>FALSE);$$, time_interval);

    RETURN ret;
END
$BODY$;

-- Outputs all the rows from the SETOF chunk-tables given as argument
-- Takes the output of drop_chunks (in dry run mode) as an input TODO: may change

-- NOTE: The client is responsible to pass an array of correctly escaped table names
CREATE OR REPLACE FUNCTION _timescaledb_internal.get_rows_from_tables(
    table_names TEXT[],
    table_type  anyelement
)
    RETURNS SETOF anyelement LANGUAGE PLPGSQL VOLATILE AS
    $BODY$ 
    DECLARE
        t TEXT;
    BEGIN
        FOREACH t IN ARRAY table_names
        LOOP
            RETURN QUERY EXECUTE format('SELECT * from %s',t);
        END LOOP;
        RETURN;
    END
    $BODY$;

-- Outputs all the rows from the SETOF chunk-tables given as argument into a CSV file at the 
-- given path
-- Takes the output of drop_chunks (in dry run mode) as an input TODO: may change

-- NOTE: The client is responsible to pass an array of correctly escaped table names
CREATE OR REPLACE FUNCTION _timescaledb_internal.tables_to_csv(
    table_names TEXT[],
    csv_path    TEXT
)
    RETURNS VOID LANGUAGE PLPGSQL VOLATILE AS
    $BODY$ 
    DECLARE
        len INT;
        i INT;
        query TEXT = '';
    BEGIN
        len := array_upper(table_names, 1);
        FOR i IN 1..len 
        LOOP
            query := CONCAT(query, format('(SELECT * from %s)',table_names[i]));
            IF i != (len) THEN
                query := CONCAT(query, ' UNION ALL ');
            END IF;
           -- COPY (EXECUTE format('SELECT * from %s',t)) TO format('%',csv_path) WITH DELIMITER ',';
                           --COPY (SELECT * from %s) TO '%s' WITH CSV
        END LOOP;
        EXECUTE (format('COPY (%s) TO ''%s'' WITH CSV', query, csv_path));
    END
    $BODY$;

-- linear-fit stuff
DROP TABLE IF EXISTS cpu_temps;
CREATE TABLE cpu_temps (
    time bigint,
    device int,
    temp float
);

INSERT INTO cpu_temps (time, device, temp)
VALUES
(1, 1, NULL),
(1, 2, NULL),
(1, 3, NULL),
(2, 1, NULL),
(2, 2, NULL),
(2, 3, NULL),
(3, 1, 55),
(3, 2, 58),
(3, 3, 60),
(4, 2, 59),
(4, 1, 57),
(5, 1, 62),
(4, 3, 66),
(6, 1, NULL),
(6, 2, NULL),
(7, 1, 60),
(8, 2, NULL)
;

CREATE OR REPLACE FUNCTION locf_s_no_init(a anyelement, b anyelement)
RETURNS anyelement
LANGUAGE SQL
AS '
  SELECT COALESCE(b, a)
';

DROP aggregate IF EXISTS locf(anyelement);
CREATE AGGREGATE locf(anyelement) (
  SFUNC = locf_s_no_init,
  STYPE = anyelement
);

CREATE OR REPLACE FUNCTION linear_fit(tbl regclass, time_column TEXT, val TEXT,group_by TEXT) RETURNS SETOF RECORD 
LANGUAGE PLPGSQL
AS
$BODY$
BEGIN
RETURN QUERY EXECUTE format($$
With
times as (SELECT COALESCE(%I, generate_series) as tt FROM generate_series(1,10, 0.3)  FULL OUTER JOIN (SELECT DISTINCT %I from %I ) t2 
ON generate_series = %I),
extended_tbl as (select * from times 
                               LEFT JOIN (select * from %I) t2 ON 
                              %I BETWEEN tt AND 
                              tt + 0.01)
SELECT tt, temp, device, locf(%I) over (partition by %I order by tt) FROM extended_tbl;
$$, time_column, time_column, tbl, time_column, tbl, time_column, val, group_by);
END
$BODY$;
