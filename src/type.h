#ifndef _TYPE_H_
#define _TYPE_H_

#include <stdbool.h>
#include <stddef.h>

#include "array.h"


typedef struct type_ref type_ref_t;
typedef struct type_field type_field_t;
typedef struct type type_t;

DECLARE_ARRAYOF(type_field)

enum type_tag {
    TYPE_TAG_VOID,
    TYPE_TAG_ANY,
    TYPE_TAG_INT,
    TYPE_TAG_SYM,
    TYPE_TAG_BOOL,
    TYPE_TAG_CHAR,
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
        struct {
            type_ref_t subtype_ref;
        } array_f;
        struct {
            arrayof_type_field_t fields;
        } struct_f;
        struct {
            arrayof_type_field_t fields;
        } union_f;
    } u;
};

#endif