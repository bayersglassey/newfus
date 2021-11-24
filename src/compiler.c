

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"


static int _get_def_i(compiler_t *compiler, type_def_t *def) {
    for (int i = 0; i < compiler->defs.len; i++) {
        if (def == compiler->defs.elems[i]) return i;
    }

    /* Should never happen */
    fprintf(stderr, "Couldn't find index for def: %s\n", def->name);
    exit(1);
}

typedef struct sorted_def {
    /* Information about a def which is being sorted by
    compiler_sort_inplace_refs */

    type_def_t *def;

    int
        /* has_inplace_refs: quick lookup for whether this def has any
        inplace references to it from other defs */
        has_inplace_refs: 1,

        /* sorted: quick lookup for whether this def has been sorted
        (assigned a new_i value) */
        sorted: 1;

    /* Once this def has been sorted, new_i is the index it will have in
    the new compiler->defs.elems */
    size_t new_i;
} sorted_def_t;

static int compiler_sort_inplace_refs(compiler_t *compiler) {
    /* This function implements a sort of a partial ordering, compiler->defs.
    The partial ordering is this: defs should come after any defs they have an
    inplace reference to (e.g. array subdef, struct/union field). */


    /************** ALLOCATE & INITIALIZE *************/

    sorted_def_t *sorted_defs = calloc(compiler->defs.len,
        sizeof(*sorted_defs));
    if (!sorted_defs) return 1;

    /* new_defs: array which will replace compiler->defs.elems */
    type_def_t **new_defs = calloc(compiler->defs.len, sizeof(*new_defs));
    if (!new_defs) {
        free(sorted_defs);
        return 1;
    }

    /* Populate sorted_defs */
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        sorted_def_t *sorted_def = &sorted_defs[i];
        sorted_def->def = def;

        type_t *type = &def->type;
        switch (type->tag) {
            case TYPE_TAG_ARRAY: {
                type_def_t *subdef = type_get_def(type);
                if (subdef && subdef != def) {
                    sorted_defs[_get_def_i(compiler, subdef)].has_inplace_refs = 1;
                }
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    type_ref_t *ref = &field->ref;
                    type_def_t *subdef = type_get_def(&ref->type);
                    if (subdef && subdef != def) {
                        sorted_defs[_get_def_i(compiler, subdef)].has_inplace_refs = 1;
                    }
                }
                break;
            }
            case TYPE_TAG_ALIAS: {
                break;
            }
            default: break;
        }
    }

    /* Length of new_defs, in the sense of number of elements populated
    so far (when >= compiler->defs.len, we are done our breadth-first
    traversal). */
    size_t new_defs_len = 0;


    /************** THE ALGORITHM *************/

    /* Push "root" defs (ones with no references to them) onto the new_defs
    queue */
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        if (!sorted_defs[_get_def_i(compiler, def)].has_inplace_refs) continue;
        new_defs[new_defs_len++] = compiler->defs.elems[i];
    }

    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        (void) def;
    }


    /************** CLEANUP & RETURN *************/

    /* Replace compiler->defs.elems with new_defs */
    free(compiler->defs.elems);
    compiler->defs.elems = new_defs;

    /* Hooray let's all go home now */
    free(sorted_defs);
    return 0;
}

static int _compare_aliases(const void *ptr1, const void *ptr2) {
    /* For use with qsort.
    Returns -1 if def1 should come before def2, that is, if def2 is an alias
    to def1.
    Returns 1 if the reverse is true.
    Otherwise, returns 0.

    NOTE: Caller guarantees that def1 and def2 are aliases (TYPE_TAG_ALIAS).
    */
    type_def_t *def1 = * (type_def_t **) ptr1;
    type_def_t *def2 = * (type_def_t **) ptr2;

    /* Check whether def1 is an alias to def2 */
    for (const type_def_t *def = def1;;) {
        def = def->type.u.alias_f.def;
        if (def == def2) return 1;
        if (def->type.tag != TYPE_TAG_ALIAS) break;
    }

    /* Check whether def2 is an alias to def1 */
    for (const type_def_t *def = def2;;) {
        def = def->type.u.alias_f.def;
        if (def == def1) return -1;
        if (def->type.tag != TYPE_TAG_ALIAS) break;
    }

    return 0;
}

static int compiler_sort_aliases(compiler_t *compiler) {

    /* new_defs: will replace compiler->defs.elems, once we sort them in it */
    type_def_t **new_defs = malloc(compiler->defs.len * sizeof(*new_defs));
    if (!new_defs) return 1;

    /* Copy all non-alias defs into new_defs */
    size_t new_defs_len = 0;
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        if (def->type.tag == TYPE_TAG_ALIAS) continue;
        new_defs[new_defs_len++] = def;
    }

    size_t n_aliases = compiler->defs.len - new_defs_len;

    /* Copy all alias defs into new_defs (after all the non-alias defs) */
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        if (def->type.tag != TYPE_TAG_ALIAS) continue;
        new_defs[new_defs_len++] = def;
    }

    /* Sort the aliases so that they come after their dependencies */
    qsort(
        new_defs + new_defs_len - n_aliases, n_aliases,
        sizeof(*compiler->defs.elems), &_compare_aliases);

    /* Replace compiler's defs with the new, sorted ones */
    free(compiler->defs.elems);
    compiler->defs.elems = new_defs;

    return 0;
}

int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    err = lexer_load(lexer, buffer, filename);
    if (err) return err;

    err = compiler_parse_defs(compiler);
    if (err) {
        lexer_info(lexer, stderr);
        fprintf(stderr, "Failed to parse\n");
        compiler_dump(compiler, stderr);
        return err;
    }

    if (!compiler_validate(compiler)) return 2;

    /* Sort compiler->defs such that aliases come after their subdefs */
    err = compiler_sort_aliases(compiler);
    if (err) return err;

    /* Sort compiler->defs such that arrays/structs/unions come after any
    defs they have an inplace reference to. */
    //err = compiler_sort_inplace_refs(compiler);
    //if (err) return err;

    lexer_unload(lexer);
    return 0;
}
