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
#include <catalog/pg_collation.h>
#include <catalog/pg_constraint.h>
#include <catalog/pg_constraint_fn.h>
#include <access/sysattr.h> // FirstLowInvalidHeapAttributeNumber
#include <access/genam.h> // pg_constraint scan stuff
#include <utils/fmgroids.h>
#include <catalog/indexing.h>
#include <utils/ruleutils.h>

#include <utils/regproc.h>

void ct_deparse_columns(StringInfo qs, Relation table_rel);
void ct_deparse_constraints(StringInfo qs, Relation table_rel);
char *deparse_create_table(Oid reloid);
PG_FUNCTION_INFO_V1(deparse_test);

/* outward facing api for testing purposes only.
 * Depraser is generally going to be used internally.
 */
Datum
deparse_test(PG_FUNCTION_ARGS)
{
    char *final_query = NULL;
    Oid relation_oid = PG_GETARG_OID(0);
    // Q:: who frees final query?
    final_query = deparse_create_table(relation_oid);
    PG_RETURN_TEXT_P(CStringGetTextDatum(final_query));
}

char *deparse_create_table(Oid reloid) {
    StringInfo qs = makeStringInfo();
    // CREATE TABLE
    char *table_name = NULL;
    char *namespace_name = NULL;
    Relation table_rel = relation_open(reloid, AccessShareLock);

    if(table_rel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT)
        ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("Table with OID %u cannot be deparsed."
                        " TEMP and UNLOGGED tables are not supported.", reloid)));

    /*Add initial CREATE TABLE stuff to the given query string*/
    table_name = NameStr(table_rel->rd_rel->relname);
    namespace_name = get_namespace_name(table_rel->rd_rel->relnamespace);
    appendStringInfo(qs, "CREATE TABLE %s.%s\n", quote_identifier(namespace_name),
                                                 quote_identifier(table_name));

    ct_deparse_columns(qs, table_rel);

    ct_deparse_constraints(qs, table_rel);
    // Q:: Can I close relation earlier?
    relation_close(table_rel, AccessShareLock);

    return qs->data;
}

/* Deparse columns and add to the given query string*/
void
ct_deparse_columns(StringInfo qs,Relation table_rel) {
    Oid reloid = table_rel->rd_id;
    Relation pg_type;
    Relation pg_collation;
    TupleDesc rel_descr;
    FormData_pg_attribute attr;
    Form_pg_type pg_type_row;
    Form_pg_collation pg_collation_row;
    HeapTuple heaptuple;
    int dim_iter;
    int attrs_sofar = 0;
    Assert(true); // TODO:: Q:: some kind of validaiton for qs?
    if (table_rel->rd_rel->relkind != RELKIND_RELATION)
    // TODO:: Q:: should relkind cryptic letter be given a human readable alias in error message?
        ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Argument having oid %d is not a table but is %c",
                        reloid, table_rel->rd_rel->relkind)));

    rel_descr = RelationGetDescr(table_rel);// Q:: can directly access rel->rd_att like in chunk_index etc
    pg_type = relation_open(TypeRelationId, AccessShareLock);
    pg_collation = relation_open(CollationRelationId, AccessShareLock);

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
        /// ^^^^ from pg_dump. Q:: what is a binary upgrade? Sth to worry about?
        /* Format properly if not first attr */
        if (attrs_sofar == 0)
            appendStringInfoString(qs, " (");
        else
            appendStringInfoString(qs, ",");
        appendStringInfoString(qs, "\n\t");
        attrs_sofar++;

        heaptuple = get_catalog_object_by_oid(pg_type, attr.atttypid);
        pg_type_row = (Form_pg_type) GETSTRUCT(heaptuple);
        // <^^^-- not used after some refactoring, left here in case we need more type info for
        // some edge cases.

        appendStringInfoString(qs, quote_identifier(NameStr(attr.attname)));
        appendStringInfo(qs, " %s", format_type_with_typemod_qualified(attr.atttypid, attr.atttypmod));
        /*
        From https://www.postgresql.org/docs/9.1/static/arrays.html
        However, the current implementation ignores any supplied array size limits,
        i.e., the behavior is the same as for arrays of unspecified length.

        The current implementation does not enforce the declared number of dimensions either.
        Arrays of a particular element type are all considered to be of the same type,
        regardless of size or number of dimensions. So, declaring the array size or number
        of dimensions in CREATE TABLE is simply documentation; it does not affect run-time behavior.
        */
        dim_iter = 0;
        while(++dim_iter < attr.attndims) // first pair of [] comes from pg_type
            appendStringInfoString(qs, "[]");

        // technically the second condition is not necessary but this is to avoid
        // clutter in generated query
        if (attr.attcollation != InvalidOid && attr.attcollation != get_typcollation(attr.atttypid)) {
            heaptuple = get_catalog_object_by_oid(pg_collation, attr.attcollation);
            pg_collation_row = (Form_pg_collation) GETSTRUCT(heaptuple);
            // Q:: do I need to worry about collation owner, namespace etc?
            appendStringInfo(qs, " COLLATE %s", quote_identifier(NameStr(pg_collation_row->collname)));
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
                    appendStringInfo(qs, " DEFAULT %s",attr_default); // Q:: shall this be quoted? if so how?
                    // quoting it as literal makes things like this 'val'::text -> ''val'::text ''::text
                    break;
                }
            }
        }
    }

    relation_close(pg_collation, AccessShareLock);
    relation_close(pg_type, AccessShareLock);
    appendStringInfoString(qs, "\n)");
    appendStringInfoString(qs, ";\n");
}

// adapted from chunk_constraint.c:chunk_constraints_add_inheritable_constraints
/* Add constraint informaiton of the given Relation to the given StringInfo
 * as valid SQL ALTER TABLE statements.
*/
void ct_deparse_constraints(StringInfo qs, Relation table_rel) {
    Oid reloid = table_rel->rd_id;
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
        if (pg_constraint->condeferred ||pg_constraint->condeferrable)
            ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                    errmsg("Table with OID %u has deferred or deferrable constraints."
                           " These are not supported in deparsing",
                            reloid)));


        // Q:: get_relation_constraint_oid claims to return __a constraint__ and looks like names do not have to unique.
        // is this a real issue? I saw the function being used in other places that is why decided to use here
        // despite the point above.
        constroid = get_relation_constraint_oid(reloid, NameStr(pg_constraint->conname), false);
        // could use `pg_get_constraintdef_ext` if we think this is part of an internal postgres API bound to change
        // Q:: the function was renamed from `pg_get_constraintdef_string` to `pg_get_constraintdef_command` in postgres 9.5. Do we support version before?
        // https://github.com/postgres/postgres/commit/a35c5b7c1ffcde123b7b9b717608ed8357af870f
        // (funny that they did not rename pg_get_indexdef_string in the same file )
        constrdef = pg_get_constraintdef_command(constroid);
        appendStringInfo(qs, "%s;\n", constrdef);
	}

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);
}

//TODO use this in index deparsing `  pg_get_indexdef_worker