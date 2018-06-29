#include <postgres.h>
#include <catalog/namespace.h>

#include <fmgr.h>
#include <utils/rel.h>
#include <commands/dbcommands.h> //get_database_oid(dbname)
#include <utils/varlena.h> // for SQL text types not needed after SQL exposure is removed
#include <utils/lsyscache.h>
#include <utils/builtins.h>
#include <utils/syscache.h>
#include <access/htup_details.h>
#include <access/heapam.h>
#include <utils/relcache.h>

// #include "compat.h"
// #if PG10
#include <utils/regproc.h>
// #endif


// #include <
// #include <catalog/namespace.h>
// #include <nodes/value.h>
// #include <utils/lsyscache.h>
// #include <utils/datum.h>
// #include <lib/stringinfo.h>
// #include <libpq/pqformat.h>

Oid get_relation_id(char * relationName);
Oid get_relation_blah(Oid reloid);

PG_FUNCTION_INFO_V1(ngtest);

/* first(internal internal_state, anyelement value, "any" comparison_element) */
Datum
ngtest(PG_FUNCTION_ARGS)
{
    Oid reloid;

    text *relation_text = PG_GETARG_TEXT_P(0);
    char *relation_name = TextDatumGetCString(relation_text);
    reloid = get_relation_id(relation_name);
    elog(INFO,"haha %s", get_namespace_name(get_rel_namespace(reloid)));
    elog(INFO,"table reltype %d", get_relation_blah(reloid));

    PG_RETURN_INT32(33);
}




/* From emacs -nw citus/src/backend/distributed/master/master_node_protocol.c:528 */
/* Finds the relationId from a potentially qualified relation name. */
Oid
get_relation_id(char *relationName)
{
        List *relationNameList = NIL;
        RangeVar *relation = NULL;
        Oid relationId = InvalidOid;
        bool failOK = false;        /* error if relation cannot be found */

        /* resolve relationId from passed in schema and relation name */
        relationNameList = stringToQualifiedNameList(relationName);
        relation = makeRangeVarFromNameList(relationNameList);
        relationId = RangeVarGetRelid(relation, NoLock, failOK);
    
        return relationId;
}

Oid
get_relation_blah(Oid reloid) {
    HeapTuple tuple;
    Relation rel;
    TupleDesc rel_descr;
    ListCell *column;
    FormData_pg_attribute attr;

    rel = relation_open(reloid, NoLock);
    // rel_descr = RelationGetFKeyList(rel);
    rel_descr = RelationGetDescr(rel);

    for(int i = 0; i < rel_descr->natts; i++) {
        attr = (*(rel_descr->attrs)[i]);
        elog(INFO, "attr # %d, name:%s, type: %s", i, NameStr(attr.attname), NameStr(attr.attidentity));
    }

    // tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(reloid));
    // if (!HeapTupleIsValid(tuple))
	// 	ereport(ERROR,
	// 			(errcode(ERRCODE_UNDEFINED_SCHEMA),
	// 			 errmsg("table with OID %u does not exist", reloid)));
    // reltup = (Form_pg_class) GETSTRUCT(tuple);
	// ReleaseSysCache(tuple);
    relation_close(reloid, NoLock);
    return 0;
}

        /* create schema if the table is not in the default namespace (public) */
        // schemaId = get_rel_namespace(relationId);
        // createSchemaCommand = CreateSchemaDDLCommand(schemaId);
        // if (createSchemaCommand != NULL)
        // {
        //         tableDDLEventList = lappend(tableDDLEventList, createSchemaCommand);
        // }
