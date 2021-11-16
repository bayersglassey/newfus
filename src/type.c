

#include <stdbool.h>
#include <stdlib.h>

#include "type.h"

void type_ref_cleanup(type_ref_t *ref) {
    type_cleanup(ref->type);
}
void type_field_cleanup(type_field_t *field) {
    type_ref_cleanup(&field->type_ref);
}
void type_u_array_cleanup(type_u_array_t *array_f) {
    type_ref_cleanup(&array_f->subtype_ref);
}
void type_u_struct_cleanup(type_u_struct_t *struct_f) {
    ARRAY_FREE(struct_f->fields, type_field_cleanup)
}
void type_u_union_cleanup(type_u_union_t *union_f) {
    ARRAY_FREE(union_f->fields, type_field_cleanup)
}
void type_cleanup(type_t *type) {
    switch (type->tag) {
        case TYPE_TAG_ARRAY:
            type_u_array_cleanup(&type->u.array_f);
            break;
        case TYPE_TAG_STRUCT:
            type_u_struct_cleanup(&type->u.struct_f);
            break;
        case TYPE_TAG_UNION:
            type_u_union_cleanup(&type->u.union_f);
            break;
        default: break;
    }
}

