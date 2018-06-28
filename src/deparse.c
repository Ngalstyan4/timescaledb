#include <postgres.h>
#include <catalog/namespace.h>

#include <fmgr.h>
#include <utils/rel.h>
#include <commands/dbcommands.h> //get_database_oid(dbname)
#include <utils/varlena.h> // textToQualifiedNameList

#include <utils/builtins.h>

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

PG_FUNCTION_INFO_V1(ngtest);

/* first(internal internal_state, anyelement value, "any" comparison_element) */
Datum
ngtest(PG_FUNCTION_ARGS)
{
    text *relation_text = PG_GETARG_TEXT_P(0);
    char *relation_name = TextDatumGetCString(relation_text);
    elog(INFO,"haha %d", get_relation_id(relation_name));
    PG_RETURN_INT32(get_relation_id(relation_name));
}




/******* COPY-PASTE START *********/
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

        /* create schema if the table is not in the default namespace (public) */
        // schemaId = get_rel_namespace(relationId);
        // createSchemaCommand = CreateSchemaDDLCommand(schemaId);
        // if (createSchemaCommand != NULL)
        // {
        //         tableDDLEventList = lappend(tableDDLEventList, createSchemaCommand);
        // }

/******* COPY-PASTE START *********/