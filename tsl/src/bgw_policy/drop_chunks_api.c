/*
 * This file and its contents are licensed under the Timescale License.
 * Please see the included NOTICE for copyright information and
 * LICENSE-TIMESCALE for a copy of the license.
 */

#include <postgres.h>
#include <catalog/pg_type.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>
#include <utils/lsyscache.h>
#include <utils/syscache.h>

#include <hypertable_cache.h>

#include "bgw/job.h"
#include "bgw_policy/drop_chunks.h"
#include "drop_chunks_api.h"
#include "errors.h"
#include "hypertable.h"
#include "license.h"
#include "utils.h"

/* Default scheduled interval for drop_chunks jobs is currently 1 day (24 hours) */
#define DEFAULT_SCHEDULE_INTERVAL                                                                  \
	DatumGetIntervalP(DirectFunctionCall7(make_interval,                                           \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(1),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Float8GetDatum(0)))
/* Default max runtime for a drop_chunks job should not be very long. Right now set to 5 minutes */
#define DEFAULT_MAX_RUNTIME                                                                        \
	DatumGetIntervalP(DirectFunctionCall7(make_interval,                                           \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(5),                                        \
										  Float8GetDatum(0)))
/* Right now, there is an infinite number of retries for drop_chunks jobs */
#define DEFAULT_MAX_RETRIES -1
/* Default retry period for drop_chunks_jobs is currently 12 hours */
#define DEFAULT_RETRY_PERIOD                                                                       \
	DatumGetIntervalP(DirectFunctionCall7(make_interval,                                           \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(0),                                        \
										  Int32GetDatum(12),                                       \
										  Int32GetDatum(0),                                        \
										  Float8GetDatum(0)))

Datum
drop_chunks_add_policy(PG_FUNCTION_ARGS)
{
	NameData application_name;
	NameData drop_chunks_name;
	int32 job_id;
	BgwPolicyDropChunks *existing;
	Hypertable *hypertable;
	Cache *hcache;
	Oid ht_oid = PG_GETARG_OID(0);
	Datum older_than_datum = PG_GETARG_DATUM(1);
	bool cascade = PG_GETARG_BOOL(2);
	bool if_not_exists = PG_GETARG_BOOL(3);
	bool cascade_to_materializations = PG_GETARG_BOOL(4);
	Oid older_than_type = PG_ARGISNULL(1) ? InvalidOid : get_fn_expr_argtype(fcinfo->flinfo, 1);
	Interval *older_than_interval;
	int64 older_than_integer;
	Oid partitioning_type;
	BgwPolicyDropChunks policy;
	Dimension* open_dim;

	license_enforce_enterprise_enabled();
	license_print_expiration_warning_if_needed();

	hcache = ts_hypertable_cache_pin();
	hypertable = ts_hypertable_cache_get_entry(hcache, ht_oid);
	/* First verify that the hypertable corresponds to a valid table */
	if (hypertable == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_TS_HYPERTABLE_NOT_EXIST),
				 errmsg("could not add drop_chunks policy because \"%s\" is not a hypertable",
						get_rel_name(ht_oid))));

	/* validate that the open dimension uses a time type */
	open_dim = hyperspace_get_open_dimension(hypertable->space, 0);
	partitioning_type =
		ts_dimension_get_partition_type(open_dim);

	switch (older_than_type) {
		case INTERVALOID:
			ts_dimension_open_typecheck(INTERVALOID, partitioning_type, "add_drop_chunks_policy");
			older_than_interval = PG_GETARG_INTERVAL_P(1);
			older_than_integer = -1;
		break;
		case INT2OID:
		case INT4OID:
		case INT8OID:
			if (!IS_INTEGER_TYPE(partitioning_type))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("integer older_than argument can only be used with hypertables "
						"that have integer time dimensions")));// <<-- time columns? open dimensions? TODO::

			/* awkward way to check if a NameStr col is null or not but did not find a better API */
			if ('\0' == *NameStr(open_dim->fd.integer_now_func) || '\0' == *NameStr(open_dim->fd.integer_now_func_schema))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("integer older_than argument can only be used with hypertables "
						"that have integer time dimensions and when integer now function is set"),
						errhint("set interger_now function for hypertable \"%s\" using "
						"set_integer_now_func(hypertable REGCLASS, integer_now_func regproc) function",
						 get_rel_name(ht_oid))));
			older_than_interval = NULL;
			older_than_integer = ts_time_value_to_internal(older_than_datum, older_than_type);

		break;
		case InvalidOid:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("older_than argument to add_drop_chunks_policy cannot be NULL")));
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("older_than argument to add_drop_chunks_policy should be an interval or an integer")));

	}


	/* Make sure that an existing policy doesn't exist on this hypertable */
	existing = ts_bgw_policy_drop_chunks_find_by_hypertable(hypertable->fd.id);

	if (existing != NULL)
	{
		bool same_interval;
		bool same_integer;
		if (!if_not_exists)
		{
			ts_cache_release(hcache);
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("drop chunks policy already exists for hypertable \"%s\"",
							get_rel_name(ht_oid))));
		}

		same_interval = older_than_interval == NULL || DatumGetBool(DirectFunctionCall2(interval_eq,
											  IntervalPGetDatum(&existing->fd.older_than_interval),
											  older_than_datum));
		same_integer = older_than_interval != NULL || older_than_integer == existing->fd.older_than_integer;
		if (! same_interval || ! same_integer ||
			(existing->fd.cascade != cascade) ||
			(existing->fd.cascade_to_materializations != cascade_to_materializations))
		{
			elog(WARNING,
				 "could not add drop_chunks policy due to existing policy on hypertable with "
				 "different arguments");
			ts_cache_release(hcache);
			return -1;
		}

		/* If all arguments are the same, do nothing */
		ereport(NOTICE,
				(errmsg("drop chunks policy already exists on hypertable \"%s\", skipping",
						get_rel_name(ht_oid))));
		ts_cache_release(hcache);
		return -1;
	}

	ts_cache_release(hcache);

	/* Next, insert a new job into jobs table */
	namestrcpy(&application_name, "Drop Chunks Background Job");
	namestrcpy(&drop_chunks_name, "drop_chunks");
	job_id = ts_bgw_job_insert_relation(&application_name,
										&drop_chunks_name,
										DEFAULT_SCHEDULE_INTERVAL,
										DEFAULT_MAX_RUNTIME,
										DEFAULT_MAX_RETRIES,
										DEFAULT_RETRY_PERIOD);

	policy = (BgwPolicyDropChunks) { .fd = {
						.job_id = job_id,
						.hypertable_id = ts_hypertable_relid_to_id(ht_oid),
						.interval_support = older_than_interval != NULL,
						.older_than_integer = older_than_integer,
						.cascade = cascade,
						.cascade_to_materializations = cascade_to_materializations,
					}
					};
	if (older_than_interval != NULL)
		policy.fd.older_than_interval = *older_than_interval;

	/* Now, insert a new row in the drop_chunks args table */
	ts_bgw_policy_drop_chunks_insert(&policy);

	PG_RETURN_INT32(job_id);
}

Datum
drop_chunks_remove_policy(PG_FUNCTION_ARGS)
{
	Oid hypertable_oid = PG_GETARG_OID(0);
	bool if_exists = PG_GETARG_BOOL(1);

	/* Remove the job, then remove the policy */
	int ht_id = ts_hypertable_relid_to_id(hypertable_oid);
	BgwPolicyDropChunks *policy = ts_bgw_policy_drop_chunks_find_by_hypertable(ht_id);

	license_enforce_enterprise_enabled();
	license_print_expiration_warning_if_needed();

	if (policy == NULL)
	{
		if (!if_exists)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("cannot remove drop chunks policy, no such policy exists")));
		else
		{
			ereport(NOTICE,
					(errmsg("drop chunks policy does not exist on hypertable \"%s\", skipping",
							get_rel_name(hypertable_oid))));
			PG_RETURN_NULL();
		}
	}

	ts_bgw_job_delete_by_id(policy->fd.job_id);

	PG_RETURN_NULL();
}
