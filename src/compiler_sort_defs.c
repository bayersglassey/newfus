

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
typedef struct sorted_def sorted_def_t;

/* What to do when visiting a def during a traversal of a partial ordering
of defs */
typedef void visit_sorted_def_t(sorted_def_t *sorted_def);

/* A *step* of a breadth-first traversal of a partial ordering of defs; in
particular, this implements the logic for finding a def's children within
the partial ordering */
typedef void visit_sorted_def_children_t(compiler_t *compiler,
    type_def_t *def, sorted_def_t *sorted_def,
    sorted_def_t *sorted_defs, visit_sorted_def_t *visit_sorted_def);



/************** STRUCTS *******************/

struct sorter {

    /* Pointer to the end of new_defs, so that we can push defs onto the
    end of it easily, using the expression: *(--new_defs_end) = def
    NOTE: by "end" I mean... uhhh... the start. O_o
    But we're treating new_defs as a stack which fills up from the right.
    So new_defs_end is the *left* side of the stack, where things are pushed.
    See: sorted_def_sort */
    type_def_t **new_defs_end;

};

struct sorted_def {
    /* Information about a def which is being sorted */

    sorter_t *sorter;

    type_def_t *def;

    int
        /* non_root: quick lookup for whether this def has any
        references to it from other defs */
        non_root: 1,

        /* sorted: quick lookup for whether this def has been sorted
        (assigned a new_i value) */
        sorted: 1;

    /* Once this def has been sorted, new_i is the index it will have in
    the new compiler->defs.elems */
    size_t new_i;
};




/************** STATIC PROTOTYPES *******************/

static visit_sorted_def_t visit_sorted_def_mark_non_root;
static visit_sorted_def_t visit_sorted_def_attempt_sort;

static visit_sorted_def_children_t visit_inplace_refs;



/************** STATIC FUNCTIONS *******************/

static int _get_def_i(compiler_t *compiler, type_def_t *def) {
    /* Returns the index of def within compiler->defs.elems */

    for (int i = 0; i < compiler->defs.len; i++) {
        if (def == compiler->defs.elems[i]) return i;
    }

    /* Should never happen */
    fprintf(stderr, "Couldn't find index for def: %s\n", def->name);
    exit(1);
}

static void sorted_def_sort(sorted_def_t *sorted_def) {
    /* Push sorted_def's def, and mark sorted_def as having been sorted */

    sorter_t *sorter = sorted_def->sorter;
    *(--sorter->new_defs_end) = sorted_def->def;
    sorted_def->sorted = 1;
}


/* visit_sorted_def_t */
static void visit_sorted_def_mark_non_root(sorted_def_t *sorted_def) {
    sorted_def->non_root = 1;
}

/* visit_sorted_def_t */
static void visit_sorted_def_attempt_sort(sorted_def_t *sorted_def) {
    if (!sorted_def->sorted) {
        /* Push sorted_def's def, and mark sorted_def as having been sorted */
        sorted_def_sort(sorted_def);
    }
}


/* visit_sorted_def_children_t */
static void visit_inplace_refs(compiler_t *compiler,
    type_def_t *def, sorted_def_t *sorted_def,
    sorted_def_t *sorted_defs, visit_sorted_def_t *visit_sorted_def
) {
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
                    sorted_def_t *sorted_def = &sorted_defs[
                        _get_def_i(compiler, type_def_unalias(subdef))];
                    visit_sorted_def(sorted_def);
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
                        sorted_def_t *sorted_def = &sorted_defs[
                            _get_def_i(compiler, type_def_unalias(subdef))];
                        visit_sorted_def(sorted_def);
                    }
                }
            }
            break;
        }
        default: break;
    }
}

/* visit_sorted_def_children_t */
static void visit_typedefs(compiler_t *compiler,
    type_def_t *def, sorted_def_t *sorted_def,
    sorted_def_t *sorted_defs, visit_sorted_def_t *visit_sorted_def
) {
    type_t *type = &def->type;
    switch (type->tag) {
        case TYPE_TAG_ALIAS: {
            type_def_t *subdef = type->u.alias_f.def;
            sorted_def_t *sorted_def = &sorted_defs[
                _get_def_i(compiler, subdef)];
            visit_sorted_def(sorted_def);
            break;
        }
        case TYPE_TAG_FUNC: {
            {
                type_t *subtype = type->u.func_f.ret;
                type_def_t *subdef = type_get_def(subtype);
                if (subdef) {
                    sorted_def_t *sorted_def = &sorted_defs[
                        _get_def_i(compiler, subdef)];
                    visit_sorted_def(sorted_def);
                }
            }
            break;
        }
        default: break;
    }
}


static int compiler_sort_defs(compiler_t *compiler,
    visit_sorted_def_children_t *visit_sorted_def_children
) {
    /* This function sorts compiler->defs by traversing them breadth-first,
    and reordering them to match the order in which they were visited.

    The function which defines the partial ordering is passed in as
    visit_sorted_def_children, which recursively "visits" child defs of
    the def passed to it.
    It does not *call itself* recursively, rather it implements a
    breadth-first traversal by pushing *children to be visited* onto a
    queue (see sorter->new_defs_end, which is the queue, and sorted_def_sort,
    which pushes defs onto it).

    It is assumed that compiler->defs forms a partial ordering under
    visit_sorted_def_children, or rather a forest of partial orderings (so,
    with multiple roots). */

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

    sorter_t sorter = {

        /* Used to push defs onto new_defs, starting at the right and going
        leftwards, see sorted_def_sort */
        .new_defs_end = new_defs + compiler->defs.len,

    };

    /* Populate sorted_defs, including figuring out which ones are the
    root(s) of the partial ordering(s). */
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        sorted_def_t *sorted_def = &sorted_defs[i];

        /* Populate sorted_def */
        sorted_def->sorter = &sorter;
        sorted_def->def = def;

        /* Populate other sorted_defs' sorted_def->non_root */
        visit_sorted_def_children(compiler, def, sorted_def, sorted_defs,
            &visit_sorted_def_mark_non_root);
    }


    /************** THE ALGORITHM *************/

    /* Push "root" defs (ones with no references to them) onto the new_defs
    queue */
    for (int i = compiler->defs.len - 1; i >= 0; i--) {
        type_def_t *def = compiler->defs.elems[i];
        sorted_def_t *sorted_def = &sorted_defs[_get_def_i(compiler, def)];
        if (sorted_def->non_root) continue;

        /* Push sorted_def's def, and mark sorted_def as having been sorted */
        sorted_def_sort(sorted_def);
    }

    /* Iterate over new_defs from the end, treating it as a queue.
    Each iteration represents "visiting" a def, and pushes its children
    onto the new_defs queue, to be visited in a subsequent iteration.
    At the end of this process, new_defs should be completely populated
    (and will be in the reverse order of the breadth-first traversal). */
    for (int i = compiler->defs.len - 1; i >= 0; i--) {
        sorted_def_t *sorted_def = &sorted_defs[i];
        type_def_t *def = sorted_def->def;

        /* Add defs to new_defs (using new_defs_end), by following this
        def's inplace refs */
        visit_sorted_def_children(compiler, def, sorted_def, sorted_defs,
            &visit_sorted_def_attempt_sort);
    }

    if (sorter.new_defs_end != new_defs) {
        /* This should never happen.
        If it does... maybe we want some better logging here, like... a list
        of all defs which *were* visited?..
        (I don't think we can easily detect which ones *weren't* visited) */
        fprintf(stderr,
            "Not all defs were visited during breadth-first traversal\n");
        fprintf(stderr, "(Missing %zu defs out of %zu)\n",
            sorter.new_defs_end - new_defs,
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





/************** PUBLIC FUNCTIONS *******************/

int compiler_sort_inplace_refs(compiler_t *compiler) {
    /* This function sorts compiler->defs such that each def comes after
    any defs it has an inplace reference to (e.g. array subdef,
    struct/union field). */

    return compiler_sort_defs(compiler, &visit_inplace_refs);
}

int compiler_sort_typedefs(compiler_t *compiler) {
    /* Sorts compiler->defs so that the resulting C typedefs will be in the
    correct order to avoid "error: unknown type name".
    That is, sort compiler->defs so that defs come after defs whose C typedefs
    they refer to.
    (NOTE: currently, the only types whose C typedefs can refer to other C
    typedefs are TYPE_TAG_{ALIAS,FUNS}.) */

    return compiler_sort_defs(compiler, &visit_typedefs);
}
