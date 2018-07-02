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
#include <catalog/objectaccess.h> // get_catalog_object_by_oid

// #include "compat.h"
// #if PG10
#include <utils/regproc.h>
// #endif

typedef struct QueryString {
    size_t len;
    List *strings;
} QueryString;


Oid get_relation_id(char * relationName);
void query_string_add(QueryString *qs, char *q);
char *query_string_to_string(QueryString *qs);
void query_string_create_table(QueryString *qs, char *relation_fqn);
void query_string_deparse_columns(QueryString *qs, Oid reloid);
char *deparse_create_table(char *relation_fqn);
PG_FUNCTION_INFO_V1(deparse_test);

void query_string_add(QueryString *qs, char *q) {
    size_t add_len;
    AssertArg(qs->len >= 0);
    
    add_len = strlen(q);
    Assert(qs->len + add_len > qs->len);
    qs->len += add_len;
    qs->strings = lappend(qs->strings, pstrdup(q));
}
// Q:: as of now user should free this, is it ok?
char *query_string_to_string(QueryString *qs) {
    AssertArg(qs->len >0);
    char *q_str = palloc(sizeof(char) * (qs->len + 1) );
    char *current = q_str;
    ListCell *lc;
    foreach(lc, qs->strings) {
        // Q:: namestrcpy instead? 
        current = stpcpy(current, lfirst(lc));
    }
    return q_str;
}

/* outward facing api for testing purposes only.
 * Depraser is generally going to be used internally.
 */
Datum
deparse_test(PG_FUNCTION_ARGS)
{
    char *final_query = NULL;
    text *relation_text = PG_GETARG_TEXT_P(0);
    // Q:: shall I split full name with get_rel_name,get_rel_namespace?
    char *relation_fqn = TextDatumGetCString(relation_text);
    // Q:: who frees final query?
    final_query = deparse_create_table(relation_fqn);
    elog(INFO, "*******FINAL QUERY*****: \n\n %s \n ***************************", final_query);
    PG_RETURN_TEXT_P(CStringGetTextDatum(final_query));

}

char *deparse_create_table(char *relation_fqn) {
    QueryString *qs = palloc(sizeof(QueryString));
    Oid relation_oid;
    relation_oid = get_relation_id(relation_fqn);
    qs->len = 0;
    qs->strings = NIL;
    query_string_create_table(qs, relation_fqn);
    query_string_deparse_columns(qs, relation_oid);
    return query_string_to_string(qs);
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
        // Q:: difference between NoLock and AccessShareLock?
        relationId = RangeVarGetRelid(relation, NoLock, failOK);
        return relationId;
}

/*Add initial CREATE TABLE stuff to the given query string*/
void
query_string_create_table(QueryString *qs, char *relation_fqn) {
    // pretend those options do not exist [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ]
    query_string_add(qs, "CREATE TABLE \n");
    query_string_add(qs, relation_fqn);
}

/* Deparse columns and add to the given query string*/
void
query_string_deparse_columns(QueryString *qs, Oid reloid) {
    char *col_type;
    Relation table_rel;
    TupleDesc rel_descr;
    FormData_pg_attribute attr;
    int atts_sofar = 0;
    int dim_iter;
    table_rel = relation_open(reloid, NoLock);
    Assert(true); // TODO:: Q:: some kind of validaiton for qs?
    if (table_rel->rd_rel->relkind != RELKIND_RELATION)
    // TODO:: Q:: should relkind cryptic letter be given a human readable alias in error message?
    // I can probably find
        elog(ERROR, "argument having oid %d is not a table but is %c", reloid, table_rel->rd_rel->relkind);

    rel_descr = RelationGetDescr(table_rel);// Q:: can directly access rel->rd_att like in chunk_index etc

    for(int i = 0; i < rel_descr->natts; i++) {
        attr = (*(rel_descr->attrs)[i]);

        if (attr.attisdropped)
            continue;
        /*
            * Normally, dump if it's locally defined in this table, and
            * not dropped.  But for binary upgrade, we'll dump all the
            * columns, and then fix up the dropped and nonlocal cases
            * below.
        */
        /* Format properly if not first attr */
        if (atts_sofar == 0)
            query_string_add(qs, " (");
        else
            query_string_add(qs, ",");
        query_string_add(qs, "\n    ");
        atts_sofar++;
        query_string_add(qs, NameStr(attr.attname));

        /* These macros allow the collation argument to be omitted (with a default of
        * InvalidOid, ie, no collation).  They exist mostly for backwards
        * compatibility of source code.
        ^^ Q:: looks like DirectFunctionCall is for backward compatibility, shall I still use it?*/
        col_type = DatumGetCString(DirectFunctionCall1(regtypeout, ObjectIdGetDatum(attr.atttypid)));
        // TODO:: refactor into sth like query_string_add(qs, "%s %s",NameStr(attr.attname),  col_type)
        query_string_add(qs, " ");
        query_string_add(qs, col_type);
        dim_iter = 0;
        while(++dim_iter < attr.attndims) // first pair of [] comes from pg_type
            query_string_add(qs, "[]");

        if (attr.attnotnull)
            query_string_add(qs, " NOT NULL");

        // if (attr.atthasdef)
        //     query_string_add(qs, " DEFAULT");

        // attr.atthasdef;
        // Q:: pg_type has the default. unless there is a function to do, I will need to
        // relation_open it
        // attr->attislocal sth about inheritence and dropping when parent is dropped
    }
    query_string_add(qs, "\n) ");
    // Q:: Can I close relation earlier?
    relation_close(table_rel, NoLock);
}