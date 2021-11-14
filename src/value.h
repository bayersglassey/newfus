#ifndef _VALUE_H_
#define _VALUE_H_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "array.h"
#include "type.h"


typedef union value value_t;


DECLARE_PTRARRAYOF(value)


union value {
    struct {
        type_t *type_f;
        value_t *value_f;
    } any_f;
    int int_f; /* size_t?.. */
    const char *sym_f; /* Backed by a stringstore */
    bool bool_f; /* So, not a bitfield *for dynamic values*. Compiled can be different */
    char char_f; /* Unsigned?.. or we don't care?.. */
    ptrarrayof_value_t array_f;
    value_t *struct_f;
    struct {
        int tag;
        union value *value;
    } union_f;
};


static value_t *value_get_field(value_t *value, type_t *type,
    const char *name, type_t **type_ptr
) {
    assert(type->tag == TYPE_TAG_STRUCT);
    for (int i = 0; i < type->u.struct_f.fields.len; i++) {
        type_field_t *field = &type->u.struct_f.fields.elems[i];
        if (field->name == name) {
            *type_ptr = field->type_ref.type;
            return &value->struct_f[i];
        }
    }
    return NULL;
}

#endif