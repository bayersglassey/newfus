

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

int compiler_sort_inplace_refs(compiler_t *compiler) {
    /* This function sorts compiler->defs.
    In particular, defs are sorted to come after any defs they have an
    inplace reference to (e.g. array subdef, struct/union field). */

    /* Returned at end of function */
    int err = 0;


    /************** ALLOCATE & INITIALIZE *************/

    sorted_def_t *sorted_defs = NULL;
    type_def_t **new_defs = NULL;

    sorted_defs = calloc(compiler->defs.len, sizeof(*sorted_defs));
    if (!sorted_defs) {
        err = 1;
        goto end;
    }

    /* new_defs: array which will replace compiler->defs.elems */
    new_defs = calloc(compiler->defs.size, sizeof(*new_defs));
    if (!new_defs) {
        err = 1;
        goto end;
    }

    /* Populate sorted_defs */
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        sorted_def_t *sorted_def = &sorted_defs[i];

        /* Populate sorted_def->def */
        sorted_def->def = def;

        /* Populate other sorted_defs' has_inplace_refs, by following this
        def's inplace refs */
        type_t *type = &def->type;
        switch (type->tag) {
            case TYPE_TAG_ARRAY: {
                type_ref_t *ref = type->u.array_f.subtype_ref;
                if (type_ref_is_inplace(ref)) {
                    type_def_t *subdef = type_get_def(&ref->type);
                    if (subdef) {
                        sorted_def_t *sorted_def = &sorted_defs[
                            _get_def_i(compiler, type_def_unalias(subdef))];
                        sorted_def->has_inplace_refs = 1;
                    }
                }
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    type_ref_t *ref = &field->ref;
                    if (type_ref_is_inplace(ref)) {
                        type_def_t *subdef = type_get_def(&ref->type);
                        if (subdef) {
                            sorted_def_t *sorted_def = &sorted_defs[
                                _get_def_i(compiler, type_def_unalias(subdef))];
                            sorted_def->has_inplace_refs = 1;
                        }
                    }
                }
                break;
            }
            default: break;
        }
    }


    /************** THE ALGORITHM *************/

    /* Pointer to the end of new_defs, so that we can push defs onto the
    end of it easily, using the expression: *(--new_defs_end) = def
    NOTE: by "end" I mean... uhhh... the start. O_o
    But we're treating new_defs as a stack which fills up from the right.
    So new_defs_end is the *left* side of the stack, where things are pushed.
    Do you see?.. */
    type_def_t **new_defs_end = new_defs + compiler->defs.len;

    /* Push "root" defs (ones with no references to them) onto the new_defs
    queue */
    for (int i = compiler->defs.len - 1; i >= 0; i--) {
        type_def_t *def = compiler->defs.elems[i];
        sorted_def_t *sorted_def = &sorted_defs[_get_def_i(compiler, def)];
        if (sorted_def->has_inplace_refs) continue;

        /* Push def */
        *(--new_defs_end) = def;
        sorted_def->sorted = 1;
    }

    /* Iterate over new_defs from the end, treating it as a queue, finding
    each def's inplace refs and pushing their defs onto new_defs.
    At the end of this process, new_defs should be completely populated. */
    for (int i = compiler->defs.len - 1; i >= 0; i--) {
        sorted_def_t *sorted_def = &sorted_defs[i];
        type_def_t *def = sorted_def->def;

        /* Populate other sorted_defs' has_inplace_refs, by following this
        def's inplace refs */
        type_t *type = &def->type;
        switch (type->tag) {
            case TYPE_TAG_ARRAY: {
                type_ref_t *ref = type->u.array_f.subtype_ref;
                if (type_ref_is_inplace(ref)) {
                    type_def_t *subdef = type_get_def(&ref->type);
                    if (subdef) {
                        sorted_def_t *sorted_def = &sorted_defs[
                            _get_def_i(compiler, type_def_unalias(subdef))];
                        if (!sorted_def->sorted) {
                            sorted_def->sorted = 1;
                            *(--new_defs_end) = sorted_def->def;
                        }
                    }
                }
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    type_ref_t *ref = &field->ref;
                    if (type_ref_is_inplace(ref)) {
                        type_def_t *subdef = type_get_def(&ref->type);
                        if (subdef) {
                            sorted_def_t *sorted_def = &sorted_defs[
                                _get_def_i(compiler, type_def_unalias(subdef))];
                            if (!sorted_def->sorted) {
                                sorted_def->sorted = 1;
                                *(--new_defs_end) = sorted_def->def;
                            }
                        }
                    }
                }
                break;
            }
            default: break;
        }
    }

    if (new_defs_end != new_defs) {
        /* This should never happen.
        If it does... maybe we want some better logging here, like... a list
        of all defs which *were* visited?..
        (I don't think we can easily detect which ones *weren't* visited) */
        fprintf(stderr,
            "Not all defs were visited during breadth-first traversal\n");
        fprintf(stderr, "(Missing %zu defs out of %zu)\n",
            new_defs_end - new_defs,
            compiler->defs.len);
        err = 2;
        goto end;
    }


    /************** CLEANUP & RETURN *************/

    /* Replace compiler->defs.elems with new_defs */
    free(compiler->defs.elems);
    compiler->defs.elems = new_defs;
    new_defs = NULL;

end:
    /* Hooray let's all go home now */
    free(sorted_defs);
    free(new_defs);
    return err;
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

int compiler_sort_aliases(compiler_t *compiler) {

    /* new_defs: will replace compiler->defs.elems, once we sort them in it */
    type_def_t **new_defs = calloc(compiler->defs.size, sizeof(*new_defs));
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
