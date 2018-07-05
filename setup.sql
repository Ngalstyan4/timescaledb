DO $BODY$
BEGIN
  IF NOT EXISTS (
      SELECT
        *
      FROM
        information_schema.tables
      WHERE
        table_name = 'timetable') THEN

      -- let the user know about setting things up
      RAISE NOTICE 'seting up tables and extension';
      -- this is how I check if the setup has been done or not
      -- create extension
      CREATE EXTENSION timescaledb;
    -- create table
    -- added to conveniently run this script
    -- during development
    CREATE TABLE timetable (
      time TIMESTAMP NOT NULL,
      num INT NOT NULL
);
    PERFORM
      create_hypertable ('timetable','time',if_not_exists => TRUE, chunk_time_interval => interval '1 minute');
    INSERT INTO timetable (time, num)
    VALUES (now() - (interval '2 minute'), -1),
    (now() - (interval '4 minute'), -3),
    (now(), -2),
    (now(), -3);
  END IF;
END $BODY$;
