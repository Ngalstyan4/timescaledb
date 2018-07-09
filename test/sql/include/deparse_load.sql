\c single :ROLE_SUPERUSER
CREATE SCHEMA IF NOT EXISTS "customSchema" AUTHORIZATION :ROLE_DEFAULT_PERM_USER;
\c single :ROLE_DEFAULT_PERM_USER

CREATE TABLE test1 (time timestamp, temp float8);

CREATE TABLE test2 (time timestamp NOT NULL, temp float8);

-- dimension sizes not inforced or stored internall. In postgres they are for documentation purposes only
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
CREATE TABLE test8 (time_start TIMESTAMP NOT NULL,
                    time_end   TIMESTAMP NOT NULL CHECK (time_end >= time_start),
                    val_min    INT CONSTRAINT min_less_max CHECK (val_min < val_max),
                    val_max    INT CHECK (val_min < val_max) -- Q:: can I reuse constraints by name?
);
ALTER TABLE public.test8 ADD PRIMARY KEY (time_start);
ALTER TABLE public.test8 ADD UNIQUE (time_end);
CREATE TABLE test9 (time_start TIMESTAMP NOT NULL,
                    val_min    INT,
                    val_max    INT
);
ALTER TABLE public.test9 ADD CONSTRAINT cnstr CHECK (val_min < val_max);

CREATE TABLE test10 (time_start TIMESTAMP NOT NULL,
                    "strange name"    TEXT CHECK ("strange name"  != 'specific case here') DEFAULT 'multiword name',
                    "Странное название"    INT
);

CREATE TABLE test11 (time_start TIMESTAMP NOT NULL PRIMARY KEY);

CREATE TABLE test12 (
    time TIMESTAMP REFERENCES test11(time_start),
    blah TIMESTAMP REFERENCES test11(time_start)
);
CREATE TABLE test13 (time TIMESTAMP, FOREIGN KEY (time) REFERENCES test11(time_start));

CREATE TABLE "customSchema".test14 (time timestamp, temp float8);

CREATE TABLE "customSchema"."ha ha" ("ti me" timestamp without time zone, temp float8);

