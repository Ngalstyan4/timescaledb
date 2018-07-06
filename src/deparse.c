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
#include <lib/stringinfo.h>
#include <catalog/pg_type.h>
#include <catalog/objectaddress.h>
#include <access/htup_details.h>
#include <catalog/pg_collation.h>
#include <catalog/pg_constraint.h>
#include <catalog/pg_constraint_fn.h>
#include <access/sysattr.h> // FirstLowInvalidHeapAttributeNumber
#include <access/genam.h> // pg_constraint scan stuff
#include <utils/fmgroids.h>
#include <catalog/indexing.h>
#include <utils/ruleutils.h>
// #include "compat.h"
// #if PG10
#include <utils/regproc.h>
// #endif


Oid get_relation_id(char * relationName);
void ct_deparse_columns(StringInfo qs, Relation table_rel, Oid reloid);
void ct_deparse_constraints(StringInfo qs, Relation table_rel, Oid reloid);
char *deparse_create_table(Oid reloid);
PG_FUNCTION_INFO_V1(deparse_test);

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
    Oid relation_oid = get_relation_id(relation_fqn);
    // Q:: who frees final query?
    final_query = deparse_create_table(relation_oid);
    elog(INFO, "*******FINAL QUERY*****: \n\n %s \n ***************************", final_query);
    PG_RETURN_TEXT_P(CStringGetTextDatum(" ")); // not returning as with + signs looks messy

}

char *deparse_create_table(Oid reloid) {
    StringInfo qs = makeStringInfo();
    // CREATE TABLE
    char *table_name;


    Relation table_rel;
    table_rel = relation_open(reloid, ShareLock);

/*Add initial CREATE TABLE stuff to the given query string*/
    // pretend those options do not exist [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ]
    table_name = NameStr(table_rel->rd_rel->relname); // TODO relnamespace
    appendStringInfo(qs, "CREATE TABLE %s\n", table_name);

    // Add columns START
    ct_deparse_columns(qs, table_rel, reloid);
    // Add columns END
    // Add constraints START
    ct_deparse_constraints(qs, table_rel, reloid);
    // Add constraints END
        // Q:: Can I close relation earlier?
    relation_close(table_rel, ShareLock);

    return qs->data;
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

/* Deparse columns and add to the given query string*/
void
ct_deparse_columns(StringInfo qs,Relation table_rel, Oid reloid) {
    elog(INFO, "table reloid %d", reloid); // this is 34 for CHAR(30) TODO Q:: why? 8 would make more sense, at least
    Assert(table_rel->rd_id == reloid);
    Relation pg_type;
    Relation pg_collation;
    Relation pg_constraint; // used only for checks;
    TupleDesc rel_descr;
    FormData_pg_attribute attr;
    Form_pg_type pg_type_row;
    Form_pg_collation pg_collation_row;
    Form_pg_constraint pg_constraint_row;
    HeapTuple heaptuple;
    Bitmapset *primary_key_attnos = NULL;
    Bitmapset *constraint_attnos = NULL;
    int dim_iter;
    int atts_sofar = 0;
    Assert(true); // TODO:: Q:: some kind of validaiton for qs?
    if (table_rel->rd_rel->relkind != RELKIND_RELATION)
    // TODO:: Q:: should relkind cryptic letter be given a human readable alias in error message?
        elog(ERROR, "argument having oid %d is not a table but is %c", reloid, table_rel->rd_rel->relkind);

    rel_descr = RelationGetDescr(table_rel);// Q:: can directly access rel->rd_att like in chunk_index etc
    pg_type = relation_open(TypeRelationId, ShareLock);
    pg_collation = relation_open(CollationRelationId, ShareLock);
    Oid RANDOOOOOM_OID; //todo fix
    for(int i = 0; i < rel_descr->natts; i++) {
        attr = (*(rel_descr->attrs)[i]);
        elog(INFO, "->>>>> %d", attr.atttypmod); // this is 34 for CHAR(30) TODO Q:: why? 8 would make more sense, at least

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
            appendStringInfoString(qs, " (");
        else
            appendStringInfoString(qs, ",");
        appendStringInfoString(qs, "\n    ");
        atts_sofar++;

        appendStringInfoString(qs, NameStr(attr.attname));
        heaptuple = get_catalog_object_by_oid(pg_type, attr.atttypid);

        pg_type_row = (Form_pg_type) GETSTRUCT(heaptuple);

        appendStringInfo(qs, " %s",NameStr(pg_type_row->typname));
        dim_iter = 0;
        while(++dim_iter < pg_type_row->typndims) // first pair of [] comes from pg_type
            appendStringInfoString(qs, "[]");
        // technically the second condition is not necessary but this is to avoid clutter in generated
        // query
        if (attr.attcollation != InvalidOid && attr.attcollation != get_typcollation(attr.atttypid)) {
            heaptuple = get_catalog_object_by_oid(pg_collation, attr.attcollation);
            pg_collation_row = (Form_pg_collation) GETSTRUCT(heaptuple);
            // Q:: do I need to worry about collation owner, namespace etc?
            appendStringInfo(qs, " COLLATE \"%s\"", NameStr(pg_collation_row->collname));
        }
        if (attr.attnotnull)
            appendStringInfoString(qs, " NOT NULL");

        if (attr.atthasdef) {
            // Q::: shall I optimize this?
            for(int i = 0; i < rel_descr->constr->num_defval;i++) {
                AttrDefault attr_def = rel_descr->constr->defval[i];
                if (attr_def.adnum == attr.attnum) {
                    /* These macros allow the collation argument to be omitted (with a default of
                    * InvalidOid, ie, no collation).  They exist mostly for backwards
                    * compatibility of source code.
                    ^^ Q:: looks like DirectFunctionCall is for backward compatibility, shall I still use it?*/
                    // Q:: shall I keep separate variable? shall I define it in the beginning of the function?
                    char *attr_default = TextDatumGetCString(DirectFunctionCall2(pg_get_expr,
                                                            CStringGetTextDatum(attr_def.adbin),
                                                            ObjectIdGetDatum(reloid)));
                    appendStringInfo(qs, " DEFAULT %s",attr_default);
                    break;
                }
            }
        }
        // attr.attstattarget
        // if (rel_descr->constr->num_check> 0) {
        //     heaptuple = get_catalog_object_by_oid(pg_constraint, Oid objectId)
        //     elog(INFO, "CONSTRAINTS attroid %d \n name: %s\n bin: %s", attr.attrelid, rel_descr->constr->check->ccname, rel_descr->constr->check->ccbin);
        // }

        // NICE!! pg_get_constraintdef_worker does almost all of the work for me!!
        if(primary_key_attnos && bms_is_member(attr.attnum-FirstLowInvalidHeapAttributeNumber, primary_key_attnos)) {
            appendStringInfoString(qs, " PRIMARY KEY");
        }
        // attr->attislocal sth about inheritence and dropping when parent is dropped
    }
    
    relation_close(pg_collation, ShareLock);
    relation_close(pg_type, ShareLock);
    appendStringInfoString(qs, "\n) ");

    appendStringInfoString(qs, ";\n");
}

// adapted from chunk_constraint.c:chunk_constraints_add_inheritable_constraints
void ct_deparse_constraints(StringInfo qs, Relation table_rel, Oid reloid) {
	ScanKeyData skey;
	Relation	rel;
	SysScanDesc scan;
	HeapTuple	htup;
    Oid constroid;
    char * constrdef;
	ScanKeyInit(&skey,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ, reloid);

	rel = heap_open(ConstraintRelationId, AccessShareLock);
	scan = systable_beginscan(rel, ConstraintRelidIndexId, true,
							  NULL, 1, &skey);

	while (HeapTupleIsValid(htup = systable_getnext(scan)))
	{
		Form_pg_constraint pg_constraint = (Form_pg_constraint) GETSTRUCT(htup);
        constroid = get_relation_constraint_oid(reloid, NameStr(pg_constraint->conname), false);
        // constrdef = TextDatumGetCString(DirectFunctionCall1(pg_get_constraintdef_command,
        //                                                     ObjectIdGetDatum(constroid)));
        constrdef = pg_get_constraintdef_command(constroid);
        appendStringInfo(qs, "%s\n", constrdef);
	}

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);
}


// pg_get_indexdef_worker