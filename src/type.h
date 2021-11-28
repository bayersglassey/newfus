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
void TYPE##_cleanup(TYPE##_t *it);


DECLARE_TYPE(type_def)
DECLARE_TYPE(type_ref)
DECLARE_TYPE(type_field)
DECLARE_TYPE(type_arg)

DECLARE_TYPE(type_array)
DECLARE_TYPE(type_struct)
DECLARE_TYPE(type_alias)
DECLARE_TYPE(type_func)
DECLARE_TYPE(type_extern)
DECLARE_TYPE(type)

typedef ARRAYOF(type_field_t) arrayof_inplace_type_field_t;
typedef ARRAYOF(type_arg_t) arrayof_inplace_type_arg_t;


enum type_tag {
    TYPE_TAG_VOID,
    TYPE_TAG_ANY,
    TYPE_TAG_TYPE,
    TYPE_TAG_INT,
    TYPE_TAG_ERR,
    TYPE_TAG_STRING,
    TYPE_TAG_BOOL,
    TYPE_TAG_BYTE,
    TYPE_TAG_ARRAY,
    TYPE_TAG_STRUCT,
    TYPE_TAG_UNION,
    TYPE_TAG_ALIAS,
    TYPE_TAG_FUNC,
    TYPE_TAG_EXTERN,
    TYPE_TAG_UNDEFINED, /* For use while compiling, otherwise invalid */
    TYPE_TAGS
};



/* Whether references to this kind of type are pointers by default */
static bool type_tag_is_pointer(int tag) {
    return
        tag == TYPE_TAG_STRUCT ||
        tag == TYPE_TAG_UNION ||
        tag == TYPE_TAG_FUNC ||
        tag == TYPE_TAG_EXTERN;
}

/* Whether this kind of type supports "inplace" (non-pointer) references
(only makes sense for types for which type_tag_is_pointer is true) */
static bool type_tag_supports_inplace(int tag) {
    /* Function types can't be instantiated directly, you can only make
    function pointers out of them. */
    return type_tag_is_pointer(tag) && tag != TYPE_TAG_FUNC;
}

/* Whether this kind of type has a cleanup function associated with it */
static bool type_tag_has_cleanup(int tag) {
    return
        tag == TYPE_TAG_ANY ||
        tag == TYPE_TAG_ARRAY ||
        tag == TYPE_TAG_STRUCT ||
        tag == TYPE_TAG_UNION;
}

static const char *type_tag_string(int tag) {
    switch (tag) {
        case TYPE_TAG_VOID: return "void";
        case TYPE_TAG_ANY: return "any";
        case TYPE_TAG_TYPE: return "type";
        case TYPE_TAG_INT: return "int";
        case TYPE_TAG_ERR: return "err";
        case TYPE_TAG_STRING: return "string";
        case TYPE_TAG_BOOL: return "bool";
        case TYPE_TAG_BYTE: return "byte";
        case TYPE_TAG_ARRAY: return "array";
        case TYPE_TAG_STRUCT: return "struct";
        case TYPE_TAG_UNION: return "union";
        case TYPE_TAG_ALIAS: return "alias";
        case TYPE_TAG_FUNC: return "func";
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
        struct type_func {
            type_def_t *def;

            type_t *ret;
            arrayof_inplace_type_arg_t args;
        } func_f;
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

/* Represents a function argument */
struct type_arg {
    const char *name;

    /* Whether this is an "out argument", that is, a pointer to type */
    int
        out  : 1;

    type_t type;
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


static type_def_t *type_get_def(type_t *type) {
    switch (type->tag) {
        case TYPE_TAG_ARRAY:
            return type->u.array_f.def;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            return type->u.struct_f.def;
        case TYPE_TAG_ALIAS:
            return type->u.alias_f.def;
        case TYPE_TAG_FUNC:
            return type->u.func_f.def;
        default: return NULL;
    }
}

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

/* Whether this type has a cleanup function associated with it */
static bool type_has_cleanup(type_t *type) {
    type_t *real_type = type_unalias(type);
    return type_tag_has_cleanup(real_type->tag);
}

/* Whether this type supports "weakref" references to it */
static bool type_supports_weakref(type_t *type) {
    /* NOTE: weakrefs are only allowed to types with a def, since the whole
    point of weakref is to disable automatic calling of the type's cleanup
    function, and only types with a def even have a cleanup function (since
    the function's name is <def_name>_cleanup) */
    type_t *real_type = type_unalias(type);
    return type_tag_is_pointer(real_type->tag) && type_get_def(real_type);
}

/* Whether references to this type are pointers by default */
static bool type_is_pointer(type_t *type) {
    type_t *real_type = type_unalias(type);
    return type_tag_is_pointer(real_type->tag);
}

static bool type_ref_is_inplace(type_ref_t *ref) {
    if (ref->is_inplace) return true;
    return !type_is_pointer(&ref->type);
}


#endif