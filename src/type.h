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

            type_def_t *def;
            arrayof_inplace_type_field_t fields;
        } struct_f;
        struct type_alias {
            type_def_t *def;
        } alias_f;
    } u;
};

const char *type_get_subtype_name(type_t *type);

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


#endif