
#include <stdio.h>

#include "compiler.h"



void _write_type_name(type_t *type, FILE *file) {
    switch (type->tag) {
        case TYPE_TAG_VOID:
            /* C doesn't like fields being declared with void, so we use
            a char instead. */
            fprintf(file, "char");
            break;
        case TYPE_TAG_ANY:
            fprintf(file, "any_t");
            break;
        case TYPE_TAG_SYM:
            fprintf(file, "const char *");
            break;
        case TYPE_TAG_BYTE:
            fprintf(file, "char"); /* unsigned char ??? */
            break;
        case TYPE_TAG_ARRAY:
            fprintf(file, "%s_t", type->u.array_f.def->name);
            break;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            fprintf(file, "%s_t", type->u.struct_f.def->name);
            break;
        case TYPE_TAG_ALIAS:
            fprintf(file, "%s_t", type->u.alias_f.def->name);
            break;
        case TYPE_TAG_UNDEFINED:
            /* If we get here, it's basically an error situation which should
            have been caught by compiler_validate. */
            fprintf(file, "XXX_UNDEFINED_XXX");
            break;
        default:
            fputs(type_tag_string(type->tag), file);
            break;
    }
}


static bool _validate_ref(type_ref_t *ref) {
    bool ok = true;

    type_t *real_type = type_unalias(&ref->type);
    bool is_pointer = type_tag_is_pointer(real_type->tag);

    if (ref->is_inplace && real_type->tag == TYPE_TAG_UNDEFINED) {
        fprintf(stderr,
            "\"inplace\" reference not allowed for undefined type\n");
        ok = false;
    }

    if (ref->is_inplace && !is_pointer) {
        fprintf(stderr, "\"inplace\" reference not allowed to: %s\n",
            type_tag_string(real_type->tag));
        ok = false;
    }

    if (ref->is_weakref && !is_pointer) {
        fprintf(stderr, "\"weakref\" reference not allowed to: %s\n",
            type_tag_string(real_type->tag));
        ok = false;
    }

    return ok;
}

bool compiler_validate(compiler_t *compiler) {
    bool ok = true;
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        switch (type->tag) {
            case TYPE_TAG_UNDEFINED: {
                if (!def->is_extern) {
                    fprintf(stderr, "Def is undefined: %s\n", def->name);
                    ok = false;
                }
                break;
            }
            case TYPE_TAG_ARRAY: {
                if (!_validate_ref(type->u.array_f.subtype_ref)) {
                    fprintf(stderr, "...while validating: %s\n", def->name);
                    ok = false;
                }
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    if (!_validate_ref(&field->ref)) {
                        fprintf(stderr, "...while validating field %s of: %s\n",
                            field->name, def->name);
                        ok = false;
                    }
                }
                break;
            }
            case TYPE_TAG_ALIAS: {
                type_def_t *subdef = type->u.alias_f.def;
                while (subdef->type.tag == TYPE_TAG_ALIAS) {
                    if (subdef == def) {
                        fprintf(stderr, "Circular definition: %s\n",
                            def->name);
                        ok = false;
                        break;
                    }
                    subdef = subdef->type.u.alias_f.def;
                }
                break;
            }
            default: break;
        }

    }
    return ok;
}

void compiler_write_hfile(compiler_t *compiler, FILE *file) {
    fputc('\n', file);

    fprintf(file, "/* FUS typedefs */\n");
    compiler_write_typedefs(compiler, file);
    fputc('\n', file);

    fprintf(file, "/* FUS enums */\n");
    compiler_write_enums(compiler, file);
    fputc('\n', file);

    fprintf(file, "/* FUS structs */\n");
    compiler_write_structs(compiler, file);
    fputc('\n', file);

    fprintf(file, "/* FUS function prototypes */\n");
    compiler_write_prototypes(compiler, file);
}

void compiler_write_cfile(compiler_t *compiler, FILE *file) {
    fputc('\n', file);

    fprintf(file, "/* FUS function definitions */\n");
    compiler_write_functions(compiler, file);
}

void compiler_write_typedefs(compiler_t *compiler, FILE *file) {
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        /* Undefined defs are expected to be defined "elsewhere", i.e. in C */
        if (type->tag == TYPE_TAG_UNDEFINED) continue;

        fprintf(file, "typedef ");
        switch (type->tag) {
            case TYPE_TAG_ARRAY:
                fprintf(file, "struct %s", type->u.array_f.def->name);
                break;
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
                fprintf(file, "struct %s", type->u.struct_f.def->name);
                break;
            default:
                _write_type_name(type, file);
                break;
        }
        fprintf(file, " %s_t;\n", def->name);
    }
}

void compiler_write_enums(compiler_t *compiler, FILE *file) {
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        if (type->tag == TYPE_TAG_UNION) {
            fprintf(file, "enum %s_tag {\n", def->name);
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                fprintf(file, "    %s,\n", field->tag_name);
            }
            fprintf(file, "    %s\n", type->u.struct_f.tags_name);
            fprintf(file, "};\n");
        }
    }
}

void _write_type_ref(type_ref_t *ref, FILE *file) {
    _write_type_name(&ref->type, file);
    if (!type_ref_is_inplace(ref)) {
        fputc('*', file);
    }
}

void compiler_write_structs(compiler_t *compiler, FILE *file) {
    /* NOTE: writes C structs for fus structs, unions, and arrays. */

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        switch (type->tag) {
            case TYPE_TAG_ARRAY: {
                fprintf(file, "struct %s {\n", def->name);
                fprintf(file, "    size_t size;\n");
                fprintf(file, "    size_t len;\n");

                fprintf(file, "    ");
                _write_type_ref(type->u.array_f.subtype_ref, file);
                fprintf(file, " *elems;\n");

                fprintf(file, "};\n");
                break;
            }
            case TYPE_TAG_STRUCT: {
                fprintf(file, "struct %s {\n", def->name);
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    fprintf(file, "    ");
                    _write_type_ref(&field->ref, file);
                    fprintf(file, " %s;\n", field->name);
                }
                fprintf(file, "};\n");
                break;
            }
            case TYPE_TAG_UNION: {
                fprintf(file, "struct %s {\n", def->name);
                fprintf(file, "    int tag; /* enum %s_tag */\n", def->name);
                fprintf(file, "    union {\n");
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    fprintf(file, "        ");
                    _write_type_ref(&field->ref, file);
                    fprintf(file, " %s;\n", field->name);
                }
                fprintf(file, "    } u;\n");
                fprintf(file, "};\n");
                break;
            }
            default: break;
        }
    }
}

void compiler_write_prototypes(compiler_t *compiler, FILE *file) {
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        /* Undefined defs are expected to be defined "elsewhere", i.e. in
        another .h file.
        We can't even try to define a prototype because the function name
        might be a macro. */
        if (type->tag == TYPE_TAG_UNDEFINED) continue;

        type_def_t *subdef = type_get_def(type);
        if (subdef) {
            if (type->tag == TYPE_TAG_ALIAS) {
                fprintf(file, "#define %s_cleanup %s_cleanup\n",
                    def->name, subdef->name);
            } else {
                fprintf(file, "void %s_cleanup(%s_t *it);\n",
                    def->name, def->name);
            }
        } else {
            fprintf(file, "#define %s_cleanup (void)\n", def->name);
        }
    }
}

static void _write_cleanup(type_def_t *def, FILE *file) {
    type_t *type = &def->type;

    /* Undefined defs are expected to be defined "elsewhere", i.e. in C */
    if (type->tag == TYPE_TAG_UNDEFINED) return;

    /* Aliases don't have their own cleanup functions, instead they use
    macros, see compiler_write_prototypes */
    if (type->tag == TYPE_TAG_ALIAS) return;

    /* Only types with a def have their own cleanup functions, others (e.g.
    int, void) use a macro which expands to the noop prefix operator
    "(void)". */
    type_def_t *subdef = type_get_def(type);
    if (!subdef) return;

    fprintf(file, "void %s_cleanup(%s_t *it) {\n",
        def->name, def->name);
    switch (type->tag) {
        case TYPE_TAG_ARRAY: {
            type_ref_t *ref = type->u.array_f.subtype_ref;
            type_def_t *subdef = type_get_def(&ref->type);
            if (subdef && !ref->is_weakref) {
                fprintf(file, "    for (int i = 0; i < it->len; i++) {\n");
                fprintf(file, "        %s_cleanup(%sit->elems[i]);\n",
                    subdef->name,
                    type_ref_is_inplace(ref)? "&": "");
                fprintf(file, "    }\n");
            }
            fprintf(file, "    free(it->elems);\n");
            break;
        }
        case TYPE_TAG_STRUCT: {
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                type_ref_t *ref = &field->ref;
                type_def_t *subdef = type_get_def(&ref->type);
                if (subdef && !ref->is_weakref) {
                    fprintf(file, "    %s_cleanup(%sit->%s);\n",
                        subdef->name,
                        type_ref_is_inplace(ref)? "&": "",
                        field->name);
                }
            }
            break;
        }
        case TYPE_TAG_UNION: {
            fprintf(file, "    switch (it->tag) {\n");
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                type_ref_t *ref = &field->ref;
                type_def_t *subdef = type_get_def(&ref->type);
                if (subdef && !ref->is_weakref) {
                    fprintf(file, "        case %s: %s_cleanup(%sit->u.%s);\n",
                        field->tag_name,
                        subdef->name,
                        type_ref_is_inplace(ref)? "&": "",
                        field->name);
                }
            }
            fprintf(file, "        default: break;\n");
            fprintf(file, "    }\n");
            break;
        }
        default: break;
    }

    /* Stay safe, kids! */
    fprintf(file, "    memset(it, 0, sizeof(*it));\n");

    fprintf(file, "}\n");
}

void compiler_write_functions(compiler_t *compiler, FILE *file) {
    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        _write_cleanup(def, file);
    }
}
