

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"


static bool _validate_ref_recurse(type_ref_t *ref, type_t *type) {
    /* Recursive step of _validate_ref (recurses through aliases) */

    if (type->tag == TYPE_TAG_ALIAS) {
        if (!_validate_ref_recurse(ref, &type->u.alias_f.def->type)) {
            fprintf(stderr, "...aliased as: %s\n", type->u.alias_f.def->name);
            return false;
        }
        return true;
    }

    bool ok = true;

    if (ref->is_inplace && !type_tag_supports_inplace(type->tag)) {
        fprintf(stderr, "\"inplace\" reference not allowed to: %s\n",
            type_tag_string(type->tag));
        ok = false;
    }

    /* NOTE: weakrefs are only allowed to types with a def, since the whole
    point of weakref is to disable automatic calling of the type's cleanup
    function, and only types with a def even have a cleanup function (since
    the function's name is <def_name>_cleanup) */
    if (ref->is_weakref && !type_tag_supports_weakref(type->tag)) {
        fprintf(stderr, "\"weakref\" reference not allowed to: %s\n",
            type_tag_string(type->tag));
        ok = false;
    }

    return ok;
}
static bool _validate_ref(type_ref_t *ref) {
    return _validate_ref_recurse(ref, &ref->type);
}

static bool _is_circular_alias(type_def_t *def, type_def_t *parent_def) {
    /* NOTE: caller guarantees parent_def->type.tag == TYPE_TAG_ALIAS */

    if (def->type.tag != TYPE_TAG_ALIAS) return false;
    if (def == parent_def) {
        fprintf(stderr, "Circular alias definition: %s\n", def->name);
        return true;
    }
    if (_is_circular_alias(def->type.u.alias_f.def, parent_def)) {
        fprintf(stderr, "...aliased as: %s\n", def->name);
        return true;
    }
    return false;
}

static bool _def_has_circular_inplace_ref(type_def_t *def, type_def_t *parent_def);
static bool _is_circular_inplace_ref(type_ref_t *ref, type_def_t *parent_def) {
    if (!ref->is_inplace) return false;

    type_def_t *def = type_get_def(&ref->type);
    if (!def) return false;

    return _def_has_circular_inplace_ref(def, parent_def);
}

static bool _def_has_circular_inplace_ref(type_def_t *def, type_def_t *parent_def) {
    if (def == parent_def) {
        fprintf(stderr, "Circular inplace reference: %s\n", def->name);
        return true;
    }

    type_t *type = &def->type;
    switch (type->tag) {
        case TYPE_TAG_ARRAY: {
            if (_is_circular_inplace_ref(type->u.array_f.subtype_ref, parent_def)) {
                fprintf(stderr, "...in: %s\n", def->name);
                return true;
            }
            break;
        }
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                if (_is_circular_inplace_ref(&field->ref, parent_def)) {
                    fprintf(stderr, "...in field %s of: %s\n",
                        field->name, def->name);
                    return true;
                }
            }
            break;
        }
        case TYPE_TAG_ALIAS: {
            if (_def_has_circular_inplace_ref(type->u.alias_f.def, parent_def)) {
                fprintf(stderr, "...aliased as: %s\n", def->name);
                return true;
            }
        }
        default: break;
    }

    return false;
}

bool compiler_validate(compiler_t *compiler) {
    bool ok = true;
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        switch (type->tag) {
            case TYPE_TAG_UNDEFINED: {
                fprintf(stderr, "Def is undefined: %s\n", def->name);
                ok = false;
                break;
            }
            case TYPE_TAG_ARRAY: {
                if (
                    !_validate_ref(type->u.array_f.subtype_ref) ||
                    _is_circular_inplace_ref(type->u.array_f.subtype_ref, def)
                ) {
                    fprintf(stderr, "...while validating: %s\n", def->name);
                    ok = false;
                }
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    if (
                        !_validate_ref(&field->ref) ||
                        _is_circular_inplace_ref(&field->ref, def)
                    ) {
                        fprintf(stderr, "...while validating field %s of: %s\n",
                            field->name, def->name);
                        ok = false;
                    }
                }
                break;
            }
            case TYPE_TAG_ALIAS: {
                if (_is_circular_alias(type->u.alias_f.def, def)) {
                    fprintf(stderr, "...while validating: %s\n", def->name);
                    ok = false;
                }
            }
            default: break;
        }

    }
    return ok;
}
