#include <postgres.h>
#include <funcapi.h>
#include <fmgr.h>
#include <utils/datum.h>
#include <catalog/pg_type.h>
#include <utils/timestamp.h>
#include <nodes/execnodes.h>
#include <executor/executor.h>
#include <access/htup_details.h>
#include <access/tupdesc.h>
#include <nodes/makefuncs.h>
#include <utils/lsyscache.h>

#include "cache.h"
#include "hypertable_cache.h"
#include "dimension.h"
#include "compat.h"


// Datum myOidFunctionCall3Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2,
// 					 Datum arg3);
Datum show_chunks_c(PG_FUNCTION_ARGS);
TS_FUNCTION_INFO_V1(show_chunks_relay);
static const int FUNC_OID = 16540;
Datum
show_chunks_relay(PG_FUNCTION_ARGS)
{
	FuncCallContext     *funcctx;
    int                  call_cntr;
    int                  max_calls;
    TupleDesc            tupdesc;
    AttInMetadata       *attinmeta;

	List *result_set;

    /* stuff done only on the first call of the function */
    if (SRF_IS_FIRSTCALL())
    {
		const int show_chunks_sql_args[] = {REGCLASSOID, ANYELEMENTOID, ANYELEMENTOID};
		const int show_chunks_sql_nargs = 3;
		Oid anyelement_resolved_type = InvalidOid;

		/* context for show_chunks SQL function call */
		FmgrInfo show_chunks_fmgrinfo;
		FunctionCallInfoData _fcinfo;
		ReturnSetInfo rsinfo;
		TupleTableSlot *slot;
		EState	   *estate = CreateExecutorState();
		TupleDesc tupledesc;
		FuncExpr *func_expr;
		Param	   *param;
		List * arg_type_list = NIL;

		Cache *hypertable_cache;
		Hypertable *ht;
		Dimension *time_dim;


		Oid table_relid =    PG_ARGISNULL(0) ? InvalidOid : PG_GETARG_OID(0);
		/* get_fn_expr_argtype degaults to UNKNOWNOID if NULL
		 * but making it InvalidOid makes the logic simpler later
		 */
		Oid older_than_type = PG_ARGISNULL(1) ? InvalidOid : get_fn_expr_argtype(fcinfo->flinfo, 1);
		Oid newer_than_type = PG_ARGISNULL(2) ? InvalidOid : get_fn_expr_argtype(fcinfo->flinfo, 2);

		if (older_than_type != InvalidOid &&
			newer_than_type != InvalidOid &&
			older_than_type != newer_than_type)
				/* Q:: maybe give current type information? */
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("older_than_type and newer_than_type should have the same type")
						));
		/* Q:: shall I do max(older_than_type, newer_than_type) ? */
		anyelement_resolved_type = older_than_type != InvalidOid ? older_than_type : InvalidOid;
		anyelement_resolved_type = newer_than_type != InvalidOid ? newer_than_type : InvalidOid;

		if (table_relid == InvalidOid) {
			/* anyelement_resolved_type here could be any of the acceptable types. This is to
			 * just make sure poly function typecheks as this is the case when we want all the
			 * chunks from all hypertables from the beginning of time..
			 */
			anyelement_resolved_type = INT8OID;
		}

		if (anyelement_resolved_type == InvalidOid) {
			hypertable_cache = hypertable_cache_pin();
			ht = hypertable_cache_get_entry(hypertable_cache, table_relid);
			/* Q:: what is the last argument ( n )of hyperspace_get_open_dimension for?
			 * looks like it makes the function return nth time dimension, but is there not
			 * supposed to be only one ?
			 * It is always called with 0 argument at that position.
			 */
			time_dim = hyperspace_get_open_dimension(ht->space, 0);
			anyelement_resolved_type = time_dim->fd.column_type;
			cache_release(hypertable_cache);
			}
		/* Q:: maybe do the typechecking of bigint, timestamp, timestamtz here? SQL does it tho
		 * create context for show_chunks SQL funciton call
		 */
		fmgr_info(FUNC_OID, &show_chunks_fmgrinfo);
		InitFunctionCallInfoData(_fcinfo, &show_chunks_fmgrinfo, show_chunks_sql_nargs, InvalidOid, NULL, NULL);
		MemSet(&rsinfo, 0, sizeof(rsinfo));
		rsinfo.type = T_ReturnSetInfo;
		rsinfo.allowedModes = SFRM_Materialize;
		rsinfo.econtext = CreateExprContext(estate);

		tupledesc = CreateTemplateTupleDesc(1, false);
		TupleDescInitEntry(tupledesc, (AttrNumber)1, NULL,REGCLASSOID, -1, 0); /*!!!??!! one indexed? really!! */
		rsinfo.expectedDesc = tupledesc;
		_fcinfo.resultinfo = (fmNodePtr) &rsinfo;
		_fcinfo.arg[0] = PG_GETARG_DATUM(0);
		_fcinfo.argnull[0] = PG_ARGISNULL(0);


		switch (anyelement_resolved_type)
		{
			case INT2OID:
			case INT4OID:
			case INT8OID:
			case TIMESTAMPOID:
			case TIMESTAMPTZOID:
				_fcinfo.arg[1] = PG_GETARG_DATUM(1);
				/* setting arg only is not enough. argnull is checked first*/
				_fcinfo.argnull[1] = PG_ARGISNULL(1);

				_fcinfo.arg[2] = PG_GETARG_DATUM(2);
				_fcinfo.argnull[2] = PG_ARGISNULL(2);
				break;
			case InvalidOid:
				if (PG_ARGISNULL(0) && PG_ARGISNULL(1) && PG_ARGISNULL(2)) {
					/* Not giving hypertable name is valid iff no time constraint is given
					 * as different hypertable could potentially have different time dimension
					 * types
					 */
					_fcinfo.argnull[1] = _fcinfo.argnull[2] = PG_ARGISNULL(2);
					break;
				}
				/* else there is an error, pass to default */
			default:
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("older_than_type and newer_than_type can only have types: bigint,"
							" timestamp with timezone and timestamp without timezone")
						));
				break;
		}

		for (int i = 0; i < show_chunks_sql_nargs; i++) {
			param = makeNode(Param);
			param->paramkind = PARAM_EXTERN;
			/* this is not used. Not sure if it is 0-indexed or 1 */
			param->paramid = i + 1;
			param->paramtype = show_chunks_sql_args[i];
			param->paramtypmod = -1;
			param->paramcollid = get_typcollation(param->paramtype);
			param->location = -1;

			if (param->paramtype == ANYELEMENTOID)
				param->paramtype = anyelement_resolved_type;

			arg_type_list = lappend(arg_type_list, param);
		}

		/* only arg_type_list is looked at so the rest are kind of not relevant; */
		func_expr = makeFuncExpr(FUNC_OID, InvalidOid, arg_type_list, InvalidOid, InvalidOid, 0);
		_fcinfo.flinfo->fn_expr = (Node *)func_expr;
		FunctionCallInvoke(&_fcinfo);

		MemoryContext   oldcontext;
        /* create a function context for cross-call persistence */
        funcctx = SRF_FIRSTCALL_INIT();
		result_set = NIL;
        /* switch to memory context appropriate for multiple function calls */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		slot = MakeSingleTupleTableSlot(rsinfo.setDesc);


		while (tuplestore_gettupleslot(rsinfo.setResult, true, false, slot))
		{
			HeapTuple	tuple = ExecFetchSlotTuple(slot);
			Datum		values[1];
			bool		nulls[1];

			heap_deform_tuple(tuple, rsinfo.setDesc, values, nulls);
			/* Q:: this works in this particular case but would not work if I needed to return a reference type
			 * is there a way to pass tuple forward and make sure it is not freed?
			 * I would expect this to happen in current setting as MemoryContextSwitchTo is called.
			 * so any pallocs called while creating slot would be in the current context???
			 */

			/* Q::  shall I check if values[0] is null? any reason that would happen?
			*
			*/
			result_set = lappend_int(result_set, values[0]);
		}

		FreeExprContext(rsinfo.econtext, false);
		FreeExecutorState(estate);

		funcctx->user_fctx = result_set;
        /* Build a tuple descriptor for our result type */
		/* not quite necessary */
        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_SCALAR)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context "
                            "that cannot accept type record")));

        MemoryContextSwitchTo(oldcontext);
    }

    /* stuff done on every call of the function */
    funcctx = SRF_PERCALL_SETUP();

    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
    attinmeta = funcctx->attinmeta;
	result_set = (List *)funcctx->user_fctx;

	/* do when there is more left to send */
    if (call_cntr < list_length(result_set))
    {
        Datum        result;
        result = ObjectIdGetDatum(list_nth_int(result_set, call_cntr));

        SRF_RETURN_NEXT(funcctx, result);
    }
    else    /* do when there is no more left */
    {
        SRF_RETURN_DONE(funcctx);
    }
}