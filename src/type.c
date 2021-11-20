

#include <stdbool.h>
#include <stdlib.h>

#include "type.h"

const char *type_get_subtype_name(type_t *type) {
    switch (type->tag) {
        case TYPE_TAG_ARRAY:
            return type->u.array_f.def->name;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            return type->u.struct_f.def->name;
        case TYPE_TAG_ALIAS:
            return type->u.alias_f.def->name;
        default: return NULL;
    }
}

void type_ref_cleanup(type_ref_t *ref) {
    type_cleanup(&ref->type);
}

void type_field_cleanup(type_field_t *field) {
    type_ref_cleanup(&field->ref);
}

void type_cleanup(type_t *type) {
    switch (type->tag) {
        case TYPE_TAG_ARRAY:
            type_array_cleanup(&type->u.array_f);
            break;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            type_struct_cleanup(&type->u.struct_f);
            break;
        default: break;
    }
}

void type_array_cleanup(type_array_t *array_f) {
    type_ref_cleanup(array_f->subtype_ref);
    free(array_f->subtype_ref);
}

void type_struct_cleanup(type_struct_t *struct_f) {
    ARRAY_FREE(struct_f->fields, type_field_cleanup)
}

void type_def_cleanup(type_def_t *def) {
    type_cleanup(&def->type);
}
