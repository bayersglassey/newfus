
#include <stdio.h>

#include "compiler.h"



void _write_type_name(compiler_t *compiler, type_t *type, FILE *file) {
    switch (type->tag) {
        case TYPE_TAG_VOID:
            /* C doesn't like fields being declared with void, so we use
            a char instead. */
            fprintf(file, "char /* void */");
            break;
        case TYPE_TAG_ANY:
            fprintf(file, "%s_t", compiler->any_type_name);
            break;
        case TYPE_TAG_TYPE:
            fprintf(file, "%s_t", compiler->type_type_name);
            break;
        case TYPE_TAG_ERR:
            fprintf(file, "int /* err */");
            break;
        case TYPE_TAG_STRING:
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
        case TYPE_TAG_FUNC:
            fprintf(file, "%s_t", type->u.alias_f.def->name);
            break;
        case TYPE_TAG_UNDEFINED:
            /* If we get here, it's basically an error situation which should
            have been caught by compiler_validate. */
            fprintf(file, "XXX_UNDEFINED_XXX");
            break;
        case TYPE_TAG_EXTERN:
            fputs(type->u.extern_f.extern_name, file);
            break;
        default:
            fputs(type_tag_string(type->tag), file);
            break;
    }
}


void compiler_write_hfile(compiler_t *compiler, FILE *file) {
    fputc('\n', file);
    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include <stdlib.h>\n");
    fprintf(file, "#include <stdbool.h>\n");
    fprintf(file, "#include <string.h>\n");

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
    fprintf(file, "/* FUS types */\n");
    compiler_write_type_declarations(compiler, file);

    fputc('\n', file);
    fprintf(file, "/* FUS function prototypes */\n");
    compiler_write_prototypes(compiler, file);
}

void compiler_write_cfile(compiler_t *compiler, FILE *file) {
    fputc('\n', file);
    fprintf(file, "/* FUS types */\n");
    compiler_write_type_definitions(compiler, file);

    fputc('\n', file);
    fprintf(file, "/* FUS function definitions */\n");
    compiler_write_functions(compiler, file);
}

void compiler_write_typedefs(compiler_t *compiler, FILE *file) {

    fprintf(file, "typedef struct %s %s_t;\n",
        compiler->any_type_name, compiler->any_type_name);
    fprintf(file, "typedef struct %s %s_t;\n",
        compiler->type_type_name, compiler->type_type_name);

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        /* Undefined defs are expected to be defined "elsewhere", i.e. in C */
        if (type->tag == TYPE_TAG_UNDEFINED) continue;

        fprintf(file, "typedef ");
        switch (type->tag) {
            case TYPE_TAG_ARRAY:
                fprintf(file, "struct %s %s_t;\n", type->u.array_f.def->name,
                    def->name);
                break;
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
                fprintf(file, "struct %s %s_t;\n", type->u.struct_f.def->name,
                    def->name);
                break;
            case TYPE_TAG_FUNC:
                fprintf(file, "void (*%s_t)();\n", def->name);
                break;
            default:
                _write_type_name(compiler, type, file);
                fprintf(file, " %s_t;\n", def->name);
                break;
        }
    }
}

void compiler_write_enums(compiler_t *compiler, FILE *file) {
    fprintf(file, "enum %s {\n",
        compiler->types_name);

    fprintf(file, "    %s_%s,\n",
        compiler->type_type_name_upper,
        compiler->any_type_name_upper);
    fprintf(file, "    %s_%s,\n",
        compiler->type_type_name_upper,
        compiler->type_type_name_upper);

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        fprintf(file, "    %s_%s,\n",
            compiler->type_type_name_upper,
            def->name_upper);
    }

    fprintf(file, "    %s\n",
        compiler->types_name_upper);
    fprintf(file, "};\n");


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

void _write_type_ref(compiler_t *compiler, type_ref_t *ref, FILE *file) {
    _write_type_name(compiler, &ref->type, file);
    if (!type_ref_is_inplace(ref)) {
        fputc('*', file);
    }
}

void compiler_write_structs(compiler_t *compiler, FILE *file) {
    /* NOTE: writes C structs for fus structs, unions, and arrays. */

    fprintf(file, "struct %s {\n", compiler->any_type_name);
    fprintf(file, "    %s_t *type;\n", compiler->type_type_name);
    fprintf(file, "    void *value;\n");
    fprintf(file, "};\n");
    fprintf(file, "struct %s {\n", compiler->type_type_name);
    fprintf(file, "    const char *name;\n");
    fprintf(file, "    /* TODO: tag and union with type's details, e.g. fields */\n");
    fprintf(file, "    void (*cleanup_voidstar)(void *it);\n");
    fprintf(file, "};\n");

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        switch (type->tag) {
            case TYPE_TAG_ARRAY: {
                fprintf(file, "struct %s {\n", def->name);
                fprintf(file, "    size_t size;\n");
                fprintf(file, "    size_t len;\n");

                fprintf(file, "    ");
                _write_type_ref(compiler, type->u.array_f.subtype_ref, file);
                fprintf(file, " *elems;\n");

                fprintf(file, "};\n");
                break;
            }
            case TYPE_TAG_STRUCT: {
                fprintf(file, "struct %s {\n", def->name);
                ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                    fprintf(file, "    ");
                    _write_type_ref(compiler, &field->ref, file);
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
                    _write_type_ref(compiler, &field->ref, file);
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

void compiler_write_type_declarations(compiler_t *compiler, FILE *file) {

    fprintf(file, "extern %s_t %s[%s]; /* Indexed by: enum %s */\n",
        compiler->type_type_name,
        compiler->types_name,
        compiler->types_name_upper,
        compiler->types_name);

}

void compiler_write_prototypes(compiler_t *compiler, FILE *file) {

    fprintf(file, "void %s_cleanup(%s_t *it);\n",
        compiler->any_type_name, compiler->any_type_name);
    fprintf(file, "void %s_cleanup_voidstar(void *it);\n",
        compiler->any_type_name);
    fprintf(file, "#define %s_cleanup (void)\n",
        compiler->type_type_name);
    fprintf(file, "#define %s_cleanup_voidstar (void)\n",
        compiler->type_type_name);

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;

        /* Undefined defs are expected to be defined "elsewhere", i.e. in
        another .h file.
        We can't even try to define a prototype because the function name
        might be a macro. */
        if (type->tag == TYPE_TAG_UNDEFINED) continue;

        if (type->tag == TYPE_TAG_UNION) {
            fprintf(file, "const char *%s_tag_string(int tag /* enum %s_tag */) {\n",
                def->name, def->name);
            fprintf(file, "    switch (tag) {\n");
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                fprintf(file, "        case %s: return \"%s\";\n",
                    field->tag_name, field->tag_name);
            }
            fprintf(file, "        default: return \"__unknown__\";\n");
            fprintf(file, "    }\n");
            fprintf(file, "}\n");
        }


        if (type_has_cleanup(type)) {
            if (type->tag == TYPE_TAG_ALIAS) {
                type_def_t *subdef = type_get_def(type);
                fprintf(file, "#define %s_cleanup %s_cleanup\n",
                    def->name, subdef->name);
                fprintf(file, "#define %s_cleanup_voidstar %s_cleanup_voidstar\n",
                    def->name, subdef->name);
            } else {
                fprintf(file, "void %s_cleanup(%s_t *it);\n",
                    def->name, def->name);
                fprintf(file, "void %s_cleanup_voidstar(void *it);\n",
                    def->name);
            }
        } else {
            fprintf(file, "#define %s_cleanup (void)\n", def->name);
            fprintf(file, "#define %s_cleanup_voidstar (void)\n", def->name);
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

    /* If this type has no cleanup function, then we have nothing to do */
    if (!type_has_cleanup(type)) return;

    fprintf(file, "void %s_cleanup(%s_t *it) {\n",
        def->name, def->name);
    switch (type->tag) {
        case TYPE_TAG_ARRAY: {
            type_ref_t *ref = type->u.array_f.subtype_ref;
            type_def_t *subdef = type_get_def(&ref->type);
            /* NOTE: we don't bother unaliasing ref->type and checking whether
            it's a pointer type etc, because that will all be taken care of by
            _write_cleanup for that type.
            That is, if ref->type doesn't have a cleanup function, it'll have
            a #define for it as (void), which is safe to output here. */
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
            if (type->u.struct_f.extra_cleanup) {
                fprintf(file, "    %s_EXTRA_CLEANUP;\n",
                    def->name_upper);
            }
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                type_ref_t *ref = &field->ref;
                type_def_t *subdef = type_get_def(&ref->type);
                /* NOTE: we don't bother unaliasing ref->type and checking whether
                it's a pointer type etc, see similar comment for TYPE_TAG_ARRAY */
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
            if (type->u.struct_f.extra_cleanup) {
                fprintf(file, "    %s_EXTRA_CLEANUP;\n",
                    def->name_upper);
            }
            fprintf(file, "    switch (it->tag) {\n");
            ARRAY_FOR(type_field_t, type->u.struct_f.fields, field) {
                type_ref_t *ref = &field->ref;
                type_def_t *subdef = type_get_def(&ref->type);
                /* NOTE: we don't bother unaliasing ref->type and checking whether
                it's a pointer type etc, see similar comment for TYPE_TAG_ARRAY */
                if (subdef && !ref->is_weakref) {
                    fprintf(file, "        case %s:\n", field->tag_name);
                    fprintf(file, "            %s_cleanup(%sit->u.%s);\n",
                        subdef->name,
                        type_ref_is_inplace(ref)? "&": "",
                        field->name);
                    fprintf(file, "            break;\n");
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
    fprintf(file, "void %s_cleanup_voidstar(void *it) { %s_cleanup((%s_t *) it); }\n",
        def->name, def->name, def->name);
}

void compiler_write_type_definitions(compiler_t *compiler, FILE *file) {

    fprintf(file, "%s_t %s[%s] = { /* Indexed by: enum %s */\n",
        compiler->type_type_name,
        compiler->types_name,
        compiler->types_name_upper,
        compiler->types_name);

    {
        /* TYPE_TAG_ANY */
        fprintf(file, "    {\n");
        fprintf(file, "        .name = \"%s\",\n",
            compiler->any_type_name);
        fprintf(file, "        .cleanup_voidstar = &%s_cleanup_voidstar,\n",
            compiler->any_type_name);
        fprintf(file, "    }\n");
    }

    {
        /* TYPE_TAG_TYPE */
        fprintf(file, "    ,{\n");
        fprintf(file, "        .name = \"%s\",\n",
            compiler->type_type_name);
        fprintf(file, "        .cleanup_voidstar = NULL,\n");
        fprintf(file, "    }\n");
    }

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        fprintf(file, "    ,{\n");
        fprintf(file, "        .name = \"%s\",\n",
            def->name);

        type_t *type = &def->type;
        if (type_has_cleanup(type)) {
            fprintf(file, "        .cleanup_voidstar = &%s_cleanup_voidstar,\n",
                def->name);
        } else {
            fprintf(file, "        .cleanup_voidstar = NULL,\n");
        }

        fprintf(file, "    }\n");
    }

    fprintf(file, "};\n");
}

void compiler_write_functions(compiler_t *compiler, FILE *file) {

    fprintf(file, "void %s_cleanup(%s_t *it) {\n",
        compiler->any_type_name, compiler->any_type_name);
    fprintf(file, "    if (!it->type || !it->type->cleanup_voidstar) return;\n");
    fprintf(file, "    it->type->cleanup_voidstar(it->value);\n");
    fprintf(file, "}\n");
    fprintf(file, "void %s_cleanup_voidstar(void *it) { %s_cleanup((%s_t *) it); }\n",
        compiler->any_type_name, compiler->any_type_name,
        compiler->any_type_name);

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        _write_cleanup(def, file);
    }
}
