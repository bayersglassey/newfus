#ifndef _TYPE_H_
#define _TYPE_H_

#include <stdbool.h>
#include <stddef.h>

#include "array.h"

#define DECLARE_TYPE(TYPE) \
typedef struct TYPE TYPE##_t; \
void TYPE##_cleanup(TYPE##_t *it);


DECLARE_TYPE(type)
DECLARE_TYPE(type_ref)
DECLARE_TYPE(type_field)
DECLARE_TYPE(type_u_array)
DECLARE_TYPE(type_u_struct)
DECLARE_TYPE(type_u_union)


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
};

struct type_ref {
    type_t *type;
    int
        inplace  : 1,
        weakref  : 1;
};

struct type_field {
    const char *name;
    type_ref_t type_ref;
};

struct type {
    int tag; /* enum type_tag */
    union {
        struct type_u_array {
            type_ref_t subtype_ref;
        } array_f;
        struct type_u_struct {
            ARRAYOF(type_field_t) fields;
        } struct_f;
        struct type_u_union {
            ARRAYOF(type_field_t) fields;
        } union_f;
    } u;
};

#endif