/*
 * This file and its contents are licensed under the Apache License 2.0.
 * Please see the included NOTICE for copyright information and
 * LICENSE-APACHE for a copy of the license.
 */

#include <postgres.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>
#include <utils/lsyscache.h>
#include <utils/syscache.h>

#include "catalog.h"
#include "policy.h"
#include "drop_chunks.h"
#include "scanner.h"
#include "utils.h"
#include "hypertable.h"
#include "bgw/job.h"
#include "scan_iterator.h"

static ScanTupleResult
bgw_policy_drop_chunks_tuple_found(TupleInfo *ti, void *const data)
{
	BgwPolicyDropChunks **policy = data;
	bool isnull;
	Interval* interval;
	// todo:: q:: consider using heap_deform_tuple ??
	*policy = MemoryContextAllocZero(ti->mctx, sizeof(BgwPolicyDropChunks));
	(*policy)->fd.job_id = DatumGetInt32(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_job_id, ti->desc, &isnull));
	Assert(!isnull);
	(*policy)->fd.hypertable_id = DatumGetInt32(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_hypertable_id, ti->desc, &isnull));
	Assert(!isnull);
	(*policy)->fd.interval_support = DatumGetBool(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_interval_support, ti->desc, &isnull));
	Assert(!isnull);

	interval = DatumGetIntervalP(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_older_than_interval, ti->desc, &isnull));
	if(isnull) {
		(*policy)->fd.interval_support = false;
		(*policy)->fd.older_than_integer = DatumGetInt64(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_older_than_integer, ti->desc, &isnull));
	} else {
		(*policy)->fd.interval_support = true;
		(*policy)->fd.older_than_interval = *interval;
	}

	(*policy)->fd.cascade = DatumGetBool(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_cascade, ti->desc, &isnull));
	Assert(!isnull);
	(*policy)->fd.cascade_to_materializations = DatumGetBool(heap_getattr(ti->tuple, Anum_bgw_policy_drop_chunks_cascade_to_materializations, ti->desc, &isnull));
	Assert(!isnull);

	//Q:: why scan continue? Is it not wrong to have multiple? And even if there are multiple, this
	// would just take the last one?
	return SCAN_CONTINUE;
}

/*
 * To prevent infinite recursive calls from the job <-> policy tables, we do not cascade deletes in
 * this function. Instead, the caller must be responsible for making sure that the delete cascades
 * to the job corresponding to this policy.
 */
bool
ts_bgw_policy_drop_chunks_delete_row_only_by_job_id(int32 job_id)
{
	ScanKeyData scankey[1];

	ScanKeyInit(&scankey[0],
				Anum_bgw_policy_drop_chunks_pkey_job_id,
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(job_id));

	return ts_catalog_scan_one(BGW_POLICY_DROP_CHUNKS,
							   BGW_POLICY_DROP_CHUNKS_PKEY,
							   scankey,
							   1,
							   ts_bgw_policy_delete_row_only_tuple_found,
							   RowExclusiveLock,
							   BGW_POLICY_DROP_CHUNKS_TABLE_NAME,
							   NULL);
}

BgwPolicyDropChunks *
ts_bgw_policy_drop_chunks_find_by_job(int32 job_id)
{
	ScanKeyData scankey[1];
	BgwPolicyDropChunks *ret = NULL;

	ScanKeyInit(&scankey[0],
				Anum_bgw_policy_drop_chunks_pkey_job_id,
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(job_id));

	ts_catalog_scan_one(BGW_POLICY_DROP_CHUNKS,
						BGW_POLICY_DROP_CHUNKS_PKEY,
						scankey,
						1,
						bgw_policy_drop_chunks_tuple_found,
						RowExclusiveLock,
						BGW_POLICY_DROP_CHUNKS_TABLE_NAME,
						(void *) &ret);

	return ret;
}

BgwPolicyDropChunks *
ts_bgw_policy_drop_chunks_find_by_hypertable(int32 hypertable_id)
{
	ScanKeyData scankey[1];
	BgwPolicyDropChunks *ret = NULL;

	ScanKeyInit(&scankey[0],
				Anum_bgw_policy_drop_chunks_hypertable_id_key_hypertable_id,
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(hypertable_id));

	ts_catalog_scan_one(BGW_POLICY_DROP_CHUNKS,
						BGW_POLICY_DROP_CHUNKS_HYPERTABLE_ID_KEY,
						scankey,
						1,
						bgw_policy_drop_chunks_tuple_found,
						RowExclusiveLock,
						BGW_POLICY_DROP_CHUNKS_TABLE_NAME,
						(void *) &ret);

	return ret;
}

static void
ts_bgw_policy_drop_chunks_insert_with_relation(Relation rel, BgwPolicyDropChunks *policy)
{
	TupleDesc tupdesc;
	CatalogSecurityContext sec_ctx;
	Datum values[Natts_bgw_policy_drop_chunks];
	bool nulls[Natts_bgw_policy_drop_chunks] = { false };

	tupdesc = RelationGetDescr(rel);

	values[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_job_id)] =
		Int32GetDatum(policy->fd.job_id);
	values[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_hypertable_id)] =
		Int32GetDatum(policy->fd.hypertable_id);

	if (policy->fd.interval_support) {
		values[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_older_than_interval)] =
			IntervalPGetDatum(&policy->fd.older_than_interval);
		nulls[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_older_than_integer)] = true;
	} else {
		values[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_older_than_integer)] =
			Int64GetDatum(policy->fd.older_than_integer);
		nulls[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_older_than_interval)] = true;
	}

	values[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_cascade)] =
		BoolGetDatum(policy->fd.cascade);
	values[AttrNumberGetAttrOffset(Anum_bgw_policy_drop_chunks_cascade_to_materializations)] =
		BoolGetDatum(policy->fd.cascade_to_materializations);

	ts_catalog_database_info_become_owner(ts_catalog_database_info_get(), &sec_ctx);
	ts_catalog_insert_values(rel, tupdesc, values, nulls);
	ts_catalog_restore_user(&sec_ctx);
}

void
ts_bgw_policy_drop_chunks_insert(BgwPolicyDropChunks *policy)
{
	Catalog *catalog = ts_catalog_get();
	Relation rel =
		heap_open(catalog_get_table_id(catalog, BGW_POLICY_DROP_CHUNKS), RowExclusiveLock);

	ts_bgw_policy_drop_chunks_insert_with_relation(rel, policy);
	heap_close(rel, RowExclusiveLock);
}

TSDLLEXPORT int32
ts_bgw_policy_drop_chunks_count()
{
	int32 count = 0;
	ScanIterator iterator =
		ts_scan_iterator_create(BGW_POLICY_DROP_CHUNKS, AccessShareLock, CurrentMemoryContext);
	ts_scanner_foreach(&iterator) { count++; }

	return count;
}
