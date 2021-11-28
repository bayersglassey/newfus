

/* Do you like implementing graph algorithms in C?..
With "generics" implemented with function pointers?..
You monster.
Come right on in. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"



/************** TYPEDEFS *******************/

typedef struct sorter sorter_t;

/* Calls sorter_visit on children of def */
typedef int visit_children_t(sorter_t *sorter, type_def_t *def);



/************** STRUCTS *******************/

struct sorter {

    compiler_t *compiler;

    visit_children_t *visit_children;

    /* Pointer to the end of new_defs, so that we can push defs onto it
    easily, using the expression: *(new_defs_end++) = def
    See: sorter_push */
    type_def_t **new_defs_end;

};

void sorter_push(sorter_t *sorter, type_def_t *def) {
    *(sorter->new_defs_end++) = def;
}



/************** STATIC FUNCTIONS *******************/

static int sorter_visit(sorter_t *sorter, type_def_t *def);


static visit_children_t visit_inplace_refs;
static int visit_inplace_refs(sorter_t *sorter, type_def_t *def) {
    int err;
    type_t *type = &def->type;
    switch (type->tag) {
        case TYPE_TAG_ARRAY: {
            type_ref_t *ref = type->u.array_f.subtype_ref;
            if (type_ref_is_inplace(ref)) {
                type_def_t *subdef = type_get_def(&ref->type);
                if (subdef) {
                    /* NOTE: we use type_def_unalias here, because we're
                    ultimately trying to sort C struct definitions, not
                    C typedefs. */
                    err = sorter_visit(sorter, type_def_unalias(subdef));
                    if (err) {
                        fprintf(stderr, "...in subtype of %s: %s\n",
                            type_tag_string(type->tag), def->name);
                        return err;
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
                        /* NOTE: we use type_def_unalias here, because we're
                        ultimately trying to sort C struct definitions, not
                        C typedefs. */
                        err = sorter_visit(sorter, type_def_unalias(subdef));
                        if (err) {
                            fprintf(stderr, "...in field %s of %s: %s\n",
                                field->name, type_tag_string(type->tag), def->name);
                            return err;
                        }
                    }
                }
            }
            break;
        }
        default: break;
    }
    return 0;
}

static visit_children_t visit_typedefs;
static int visit_typedefs(sorter_t *sorter, type_def_t *def) {
    int err;
    type_t *type = &def->type;
    switch (type->tag) {
        case TYPE_TAG_ALIAS: {
            type_def_t *subdef = type->u.alias_f.def;
            err = sorter_visit(sorter, subdef);
            if (err) {
                fprintf(stderr, "...in %s: %s\n",
                    type_tag_string(type->tag), def->name);
                return err;
            }
            break;
        }
        case TYPE_TAG_FUNC: {
            {
                type_t *subtype = type->u.func_f.ret;
                type_def_t *subdef = type_get_def(subtype);
                if (subdef) {
                    err = sorter_visit(sorter, subdef);
                    if (err) {
                        fprintf(stderr, "...in return value of %s: %s\n",
                            type_tag_string(type->tag), def->name);
                        return err;
                    }
                }
            }
            ARRAY_FOR(type_arg_t, type->u.func_f.args, arg) {
                type_t *subtype = &arg->type;
                type_def_t *subdef = type_get_def(subtype);
                if (subdef) {
                    err = sorter_visit(sorter, subdef);
                    if (err) {
                        fprintf(stderr,
                            "...in return value of arg %s of %s: %s\n",
                            arg->name, type_tag_string(type->tag), def->name);
                        return err;
                    }
                }
            }
            break;
        }
        default: break;
    }
    return 0;
}

static int sorter_visit(sorter_t *sorter, type_def_t *def) {
    int err;

    /* This function is based on pseudocode from:
        https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search

    ...which in turn says:
        This depth-first-search-based algorithm is the one described by
        Cormen et al. (2001); it seems to have been first described in print
        by Tarjan in 1976.

    ...in my defense, I originally went it alone, and came up with an
    algorithm for sorting a partial order by finding its root nodes,
    traversing it breadth-first, and pushing visited nodes onto the
    returned list.
    But it turns out, that only worked *most* of the time, not in certain
    edge cases (specifically, when A > B, B > C, and A > C).
    So I gave up and went to Wikipedia... */

    if (def->sorted) return 0;
    if (def->sorting) {
        fprintf(stderr, "Circular dependency: %s\n", def->name);
        return 2;
    }

    def->sorting = 1;

    err = sorter->visit_children(sorter, def);
    if (err) return err;

    def->sorting = 0;
    def->sorted = 1;

    sorter_push(sorter, def);
    return 0;
}



static int compiler_sort_defs(compiler_t *compiler,
    visit_children_t *visit_children
) {
    /* This function sorts compiler->defs by traversing them depth-first,
    and reordering them to match the order in which they were visited.

    The function which defines the partial ordering is passed in as
    visit_children, which recursively "visits" child defs of the def
    passed to it.

    It is assumed that compiler->defs forms a partial ordering under
    visit_children, or rather a forest of partial orderings (i.e.
    with multiple roots). */

    int err;


    /************** ALLOCATE & INITIALIZE *************/

    type_def_t **new_defs = NULL;

    /* new_defs: array which will replace compiler->defs.elems */
    new_defs = calloc(compiler->defs.size, sizeof(*new_defs));
    if (!new_defs) return 1;

    sorter_t sorter = {

        .compiler = compiler,

        .visit_children = visit_children,

        /* Used to push defs onto new_defs */
        .new_defs_end = new_defs,

    };

    /* Populate defs' sorting and sorted flags */
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        def->sorting = 0;
        def->sorted = 0;
    }


    /************** THE ALGORITHM *************/

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        err = sorter_visit(&sorter, def);
        if (err) return err;
    }

    if (sorter.new_defs_end != new_defs + compiler->defs.len) {
        /* This should never happen.
        If it does... maybe we want some better logging here, like... a list
        of all defs which *were* visited?..
        (I don't think we can easily detect which ones *weren't* visited) */
        fprintf(stderr,
            "Not all defs were visited by the sorting algorithm\n");
        size_t n_visited = sorter.new_defs_end - new_defs;
        fprintf(stderr, "(Visited %zu defs out of %zu, missed %zu)\n",
            n_visited, compiler->defs.len, compiler->defs.len - n_visited);
        return 2;
    }


    /************** CLEANUP & RETURN *************/

    /* Replace compiler->defs.elems with new_defs */
    free(compiler->defs.elems);
    compiler->defs.elems = new_defs;

    /* Hooray let's all go home now */
    return err;
}





/************** PUBLIC FUNCTIONS *******************/

int compiler_sort_inplace_refs(compiler_t *compiler) {
    /* This function sorts compiler->defs such that each def comes after
    any defs it has an inplace reference to (e.g. array subdef,
    struct/union field). */

    if (compiler->debug) {
        fprintf(stderr, "=== %s:\n", __func__);
    }

    return compiler_sort_defs(compiler, &visit_inplace_refs);
}

int compiler_sort_typedefs(compiler_t *compiler) {
    /* Sorts compiler->defs so that the resulting C typedefs will be in the
    correct order to avoid "error: unknown type name".
    That is, sort compiler->defs so that defs come after defs whose C typedefs
    they refer to.
    (NOTE: currently, the only types whose C typedefs can refer to other C
    typedefs are TYPE_TAG_{ALIAS,FUNS}.) */

    if (compiler->debug) {
        fprintf(stderr, "=== %s:\n", __func__);
    }

    return compiler_sort_defs(compiler, &visit_typedefs);
}
