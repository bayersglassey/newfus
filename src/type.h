#ifndef _TYPE_H_
#define _TYPE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "stringstore.h"


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
DECLARE_TYPE(type_extern)
DECLARE_TYPE(type)

typedef ARRAYOF(type_field_t) arrayof_inplace_type_field_t;


enum type_tag {
    TYPE_TAG_VOID,
    TYPE_TAG_ANY,
    TYPE_TAG_TYPE,
    TYPE_TAG_INT,
    TYPE_TAG_STRING,
    TYPE_TAG_BOOL,
    TYPE_TAG_BYTE,
    TYPE_TAG_ARRAY,
    TYPE_TAG_STRUCT,
    TYPE_TAG_UNION,
    TYPE_TAG_ALIAS,
    TYPE_TAG_EXTERN,
    TYPE_TAG_UNDEFINED, /* For use when compiling */
    TYPE_TAGS
};



/* Whether references to this kind of type are pointers by default */
static bool type_tag_is_pointer(int tag) {
    return
        tag == TYPE_TAG_STRUCT ||
        tag == TYPE_TAG_UNION ||
        tag == TYPE_TAG_EXTERN;
}

static const char *type_tag_string(int tag) {
    switch (tag) {
        case TYPE_TAG_VOID: return "void";
        case TYPE_TAG_ANY: return "any";
        case TYPE_TAG_TYPE: return "type";
        case TYPE_TAG_INT: return "int";
        case TYPE_TAG_STRING: return "string";
        case TYPE_TAG_BOOL: return "bool";
        case TYPE_TAG_BYTE: return "byte";
        case TYPE_TAG_ARRAY: return "array";
        case TYPE_TAG_STRUCT: return "struct";
        case TYPE_TAG_UNION: return "union";
        case TYPE_TAG_ALIAS: return "alias";
        case TYPE_TAG_EXTERN: return "extern";
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

            /* !!! TODO !!!
            This stuff should all be removed from type_t and moved over to
            type_def_t, or more to the point, a new struct which lives on
            only those defs which represent structs/unions.
            This is probably true of array_f and alias_f as well: we should
            try to keep type_t small, and for types which have a def, move
            extra data onto the def.
            ...although at the end of the day, these C types are only used
            when compiling, so it's not the end of the world if they're a
            bit ganky. */

            /* E.g. if struct's name is "my_struct", then tags_name
            might be "MY_STRUCT_TAGS"
            (In C, this is an enum value equal to the number of tags/fields
            in this struct/union) */
            const char *tags_name;

            /* Whether our cleanup function expects extra C code to be
            provided as a macro. */
            bool extra_cleanup;
        } struct_f;
        struct type_alias {
            type_def_t *def;
        } alias_f;
        struct type_extern {
            /* Name of a C type to be defined externally */
            const char *extern_name;
        } extern_f;
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
    const char *name_upper; /* name converted to uppercase */
    type_t type;
};


/* Follow aliases until we get to the "real" underlying type */
static type_t *type_unalias(type_t *type) {
    while (type->tag == TYPE_TAG_ALIAS) type = &type->u.alias_f.def->type;
    return type;
}

/* Follow aliases until we get to the "real" underlying def */
static type_def_t *type_def_unalias(type_def_t *def) {
    while (def->type.tag == TYPE_TAG_ALIAS) def = def->type.u.alias_f.def;
    return def;
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