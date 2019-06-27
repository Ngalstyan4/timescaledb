/*
 * This file and its contents are licensed under the Timescale License.
 * Please see the included NOTICE for copyright information and
 * LICENSE-TIMESCALE for a copy of the license.
 */

#ifndef TIMESCALEDB_TSL_BGW_POLICY_DROP_CHUNKS_API_H
#define TIMESCALEDB_TSL_BGW_POLICY_DROP_CHUNKS_API_H

#include <postgres.h>

/* User-facing API functions */
extern Datum ts_integer_from_now_func_get_datum(int64 interval, Oid time_dim_type, Oid now_func);
extern void ts_integer_now_func_validate(Oid now_func_oid, Oid open_dim_type);
extern Datum set_integer_now_func(PG_FUNCTION_ARGS);
extern Datum drop_chunks_add_policy(PG_FUNCTION_ARGS);
extern Datum drop_chunks_remove_policy(PG_FUNCTION_ARGS);

#endif /* TIMESCALEDB_TSL_BGW_POLICY_DROP_CHUNKS_API_H */
