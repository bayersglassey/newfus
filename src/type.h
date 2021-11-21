#ifndef _TYPE_H_
#define _TYPE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "stringstore.h"


typedef const char *sym_t;


#define DECLARE_TYPE(TYPE) \
typedef struct TYPE TYPE##_t; \
void TYPE##_cleanup(TYPE##_t *it); \
void TYPE##_parse(TYPE##_t *it, lexer_t *lexer, stringstore_t *store); \
void TYPE##_write(TYPE##_t *it, FILE *file, int depth);


DECLARE_TYPE(type_def)
DECLARE_TYPE(type_ref)
DECLARE_TYPE(type_field)
DECLARE_TYPE(type_array)
DECLARE_TYPE(type_struct)
DECLARE_TYPE(type_alias)
DECLARE_TYPE(type)
DECLARE_TYPE(any)

typedef ARRAYOF(type_field_t) arrayof_inplace_type_field_t;


enum type_tag {
    TYPE_TAG_VOID,
    TYPE_TAG_ANY,
    TYPE_TAG_INT,
    TYPE_TAG_SYM,
    TYPE_TAG_BOOL,
    TYPE_TAG_BYTE,
    TYPE_TAG_ARRAY,
    TYPE_TAG_STRUCT,
    TYPE_TAG_UNION,
    TYPE_TAG_ALIAS,
    TYPE_TAG_UNDEFINED, /* For use when compiling */
    TYPE_TAGS
};



/* Whether references to this kind of type are pointers by default */
/* TODO: in compiler_write.c, we tend to check whether type_get_def is
non-NULL instead of using this function.
Is one of these approaches more correct?.. */
static bool type_tag_is_pointer(int tag) {
    return
        tag == TYPE_TAG_STRUCT ||
        tag == TYPE_TAG_UNION ||
        tag == TYPE_TAG_UNDEFINED;
}

static const char *type_tag_string(int tag) {
    switch (tag) {
        case TYPE_TAG_VOID: return "void";
        case TYPE_TAG_ANY: return "any";
        case TYPE_TAG_INT: return "int";
        case TYPE_TAG_SYM: return "sym";
        case TYPE_TAG_BOOL: return "bool";
        case TYPE_TAG_BYTE: return "byte";
        case TYPE_TAG_ARRAY: return "array";
        case TYPE_TAG_STRUCT: return "struct";
        case TYPE_TAG_UNION: return "union";
        case TYPE_TAG_ALIAS: return "alias";
        case TYPE_TAG_UNDEFINED: return "undefined";
        default: return "unknown";
    }
}



struct type {
    int tag; /* enum type_tag */
    union {
        struct type_array {
            type_def_t *def;
            type_ref_t *subtype_ref;
        } array_f;
        struct type_struct {
            /* NOTE: used for both structs and unions */

            /* PROBABLY TODO: type->u.struct_f should probably be a POINTER to
            type_struct_t, to reduce the size of type_t.
            Maybe the same should be done with type->u.array_f as well.

            IN FACT:
            array/struct/union stuff should live on their def!..
            So, type_t owns no pointers, and can be copied around freely.
            It should look like:
                struct type {
                    int tag;
                    type_def_t *def;
                };
            ...where def is only used for TYPE_TAG_{ARRAY/STRUCT/UNION/ALIAS}.

            Then, we can start adding more stuff to these defs, e.g. methods.
            And in particular, they can include a pointer to their
            cleanup/parse/write methods, so that TYPE_TAG_ANY can be made to
            work. */

            type_def_t *def;
            arrayof_inplace_type_field_t fields;

            /* E.g. if struct's name is "my_struct", then tags_name
            might be "MY_STRUCT_TAGS"
            (In C, this is an enum value equal to the number of tags/fields
            in this struct/union) */
            const char *tags_name;
        } struct_f;
        struct type_alias {
            type_def_t *def;
        } alias_f;
    } u;
};


/* Represents a reference to a type, e.g. from a struct's field */
struct type_ref {
    int
        /* NOTE: inplace and weakref only make sense for struct/union
        references; and they are mutually exclusive */
        is_inplace  : 1,
        is_weakref  : 1;

    type_t type;
};

/* Represents a field of a struct/union */
struct type_field {
    const char *name;
    type_ref_t ref;

    /* E.g. if parent struct's name is "my_struct", and field's name is
    "some_field", then tag_name might be "MY_STRUCT_TAG_SOME_FIELD" */
    const char *tag_name;
};

/* Represents a C typedef: a sort of top-level name where types can be
stored
Also used to represent struct/union tags, see type_{array,struct,union}_t's
"def" field */
struct type_def {
    const char *name;
    type_t type;

    /* Reference to a type defined elsewhere (so, TYPE_TAG_UNDEFINED
    is okay).
    I'm not sure how I feel about this... it seems specific to compiler,
    so maybe type_def_t belongs in compiler.h not type.h?..
    But then type_{array,struct,alias}_t should have const char *name
    instead of type_def_t *def, right?
    Hmmmmm. */
    bool is_extern;
};

struct any {
    /* TODO: can any's ref be inplace? No. Can it be a weakref? Yes. */
    type_ref_t type_ref;
    void *value;
};


/* Follow aliases until we get to the "real" underlying type */
static type_t *type_unalias(type_t *type) {
    while (type->tag == TYPE_TAG_ALIAS) type = &type->u.alias_f.def->type;
    return type;
}

static bool type_ref_is_inplace(type_ref_t *ref) {
    if (ref->is_inplace) return true;

    type_t *type = type_unalias(&ref->type);
    return !type_tag_is_pointer(type->tag);
}

static type_def_t *type_get_def(type_t *type) {
    switch (type->tag) {
        case TYPE_TAG_ARRAY:
            return type->u.array_f.def;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            return type->u.struct_f.def;
        case TYPE_TAG_ALIAS:
            return type->u.alias_f.def;
        default: return NULL;
    }
}


#endif