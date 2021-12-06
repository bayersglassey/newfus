
#include <stdio.h>

#include "compiler.h"



void _write_c_type(compiler_t *compiler, type_t *type, FILE *file) {
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
            fprintf(file, "const %s_t *", compiler->type_type_name);
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

void _write_type(compiler_t *compiler, type_t *type, FILE *file) {
    _write_c_type(compiler, type, file);
    if (type_is_pointer(type)) {
        fputc('*', file);
    }
}

void _write_type_ref(compiler_t *compiler, type_ref_t *ref, FILE *file) {
    _write_c_type(compiler, &ref->type, file);
    if (!type_ref_is_inplace(ref)) {
        fputc('*', file);
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

    fprintf(file, "typedef struct lexer lexer_t;\n");
    fprintf(file, "typedef struct writer writer_t;\n");

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
                _write_type(compiler, type->u.func_f.ret, file);
                fprintf(file, " %s_t(", def->name);
                for (int i = 0; i < type->u.func_f.args.len; i++) {
                    type_arg_t *arg = &type->u.func_f.args.elems[i];
                    if (i != 0) fputs(", ", file);
                    _write_type(compiler, &arg->type, file);
                    fputc(' ', file);
                    if (arg->out) fputc('*', file);
                    fprintf(file, "%s", arg->name);
                }
                fprintf(file, ");\n");
                break;
            default:
                _write_c_type(compiler, type, file);
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

void compiler_write_structs(compiler_t *compiler, FILE *file) {
    /* NOTE: writes C structs for fus structs, unions, and arrays. */

    fprintf(file, "struct %s {\n", compiler->any_type_name);
    fprintf(file, "    %s_t *type;\n", compiler->type_type_name);
    fprintf(file, "    int weakref: 1;\n");
    fprintf(file, "    union {\n");
    fprintf(file, "        char c; /* byte */\n");
    fprintf(file, "        bool b; /* bool */\n");
    fprintf(file, "        int i; /* int, err */\n");
    fprintf(file, "        const char *s; /* string */\n");
    fprintf(file, "        void (*fp)(); /* func */\n");
    fprintf(file, "        void *p; /* everything else */\n");
    fprintf(file, "    } u;\n");
    fprintf(file, "};\n");
    fprintf(file, "struct %s {\n", compiler->type_type_name);
    fprintf(file, "    const char *name;\n");
    fprintf(file, "    /* TODO: tag and union with type's details, e.g. fields */\n");
    fprintf(file, "    void (*cleanup)(void *it);\n");
    fprintf(file, "    int (*parse)(void *it, lexer_t *lexer);\n");
    fprintf(file, "    int (*write)(void *it, writer_t *writer);\n");
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

static void _write_prototypes(const char *name, type_t *type,
    FILE *file
) {
    if (type->tag == TYPE_TAG_UNION) {
        fprintf(file, "static const char *%s_tag_string(int tag /* enum %s_tag */) {\n",
            name, name);
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
                name, subdef->name);
            fprintf(file, "#define %s_cleanup_voidstar %s_cleanup_voidstar\n",
                name, subdef->name);
        } else {
            fprintf(file, "void %s_cleanup(%s_t *it);\n",
                name, name);
            fprintf(file, "void %s_cleanup_voidstar(void *it);\n",
                name);
        }
    } else {
        fprintf(file, "#define %s_cleanup (void)\n", name);
        fprintf(file, "#define %s_cleanup_voidstar (void)\n", name);
    }

    fprintf(file, "int %s_parse(%s_t *it, lexer_t *lexer);\n", name, name);
    fprintf(file, "int %s_parse_voidstar(void *it, lexer_t *lexer);\n", name);
    fprintf(file, "int %s_write(%s_t *it, writer_t *writer);\n", name, name);
    fprintf(file, "int %s_write_voidstar(void *it, writer_t *writer);\n", name);
}

void compiler_write_prototypes(compiler_t *compiler, FILE *file) {


    type_t type_any = { .tag = TYPE_TAG_ANY };
    _write_prototypes("any", &type_any, file);

    type_t type_type = { .tag = TYPE_TAG_TYPE };
    _write_prototypes("type", &type_type, file);

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        type_t *type = &def->type;
        _write_prototypes(def->name, type, file);
    }
}

static const char *_type_function_name(type_t *type) {
    /* Returns the string which, with suffixes such as "_cleanup", "_parse",
    etc, forms the names of this type's associated C functions */

    switch (type->tag) {
        case TYPE_TAG_ANY:
            return "any";
        case TYPE_TAG_ARRAY:
            return type->u.array_f.def->name;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            return type->u.struct_f.def->name;
        case TYPE_TAG_ALIAS:
            /* Not a type for which type_tag_has_cleanup is true, but caller
            must guarantee us that type_has_cleanup is true... */
            return type->u.alias_f.def->name;
        default:
            /* This is not a type for which type_tag_has_cleanup is true,
            so nobody should be calling us... */
            return "XXX_UNDEFINED_XXX";
    }
}

static void _write_cleanup_function(type_def_t *def, FILE *file) {
    type_t *type = &def->type;

    /* If this type has no cleanup function, then we have nothing to do */
    if (!type_tag_has_cleanup(type->tag)) return;

    fprintf(file, "void %s_cleanup(%s_t *it) {\n",
        def->name, def->name);
    switch (type->tag) {
        case TYPE_TAG_ARRAY: {
            type_ref_t *ref = type->u.array_f.subtype_ref;
            if (!ref->is_weakref && type_has_cleanup(&ref->type)) {
                fprintf(file, "    for (int i = 0; i < it->len; i++) {\n");
                fprintf(file, "        %s_cleanup(%sit->elems[i]);\n",
                    _type_function_name(&ref->type),
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
                if (!ref->is_weakref && type_has_cleanup(&ref->type)) {
                    fprintf(file, "    %s_cleanup(%sit->%s);\n",
                        _type_function_name(&ref->type),
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
                if (!ref->is_weakref && type_has_cleanup(&ref->type)) {
                    fprintf(file, "        case %s:\n", field->tag_name);
                    fprintf(file, "            %s_cleanup(%sit->u.%s);\n",
                        _type_function_name(&ref->type),
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

static void _write_parse_function(type_def_t *def, FILE *file) {
    //type_t *type = &def->type;

    fprintf(file, "int %s_parse(%s_t *it, lexer_t *lexer) {\n",
        def->name, def->name);
    fprintf(file, "}\n");
    fprintf(file, "int %s_parse_voidstar(void *it, lexer_t *lexer) { return %s_parse((%s_t *) it, lexer); }\n",
        def->name, def->name, def->name);
}

static void _write_write_function(type_def_t *def, FILE *file) {
    //type_t *type = &def->type;

    fprintf(file, "int %s_write(%s_t *it, writer_t *writer) {\n",
        def->name, def->name);
    fprintf(file, "}\n");
    fprintf(file, "int %s_write_voidstar(void *it, writer_t *writer) { return %s_write((%s_t *) it, writer); }\n",
        def->name, def->name, def->name);
}

static void _write_type_definition(const char *name, type_t *type,
    FILE *file, bool first
) {
    fprintf(file, "    %s{\n", first? "": ",");
    fprintf(file, "        .name = \"%s\",\n", name);
    if (type_has_cleanup(type)) {
        fprintf(file, "        .cleanup = &%s_cleanup_voidstar,\n",
            name);
    } else {
        fprintf(file, "        .cleanup = NULL,\n");
    }
    fprintf(file, "        .parse = &%s_parse_voidstar,\n", name);
    fprintf(file, "        .write = &%s_write_voidstar,\n", name);
    fprintf(file, "    }\n");
}

void compiler_write_type_definitions(compiler_t *compiler, FILE *file) {

    fprintf(file, "%s_t %s[%s] = { /* Indexed by: enum %s */\n",
        compiler->type_type_name,
        compiler->types_name,
        compiler->types_name_upper,
        compiler->types_name);

    type_t type_any = { .tag = TYPE_TAG_ANY };
    _write_type_definition("any", &type_any, file, true);

    type_t type_type = { .tag = TYPE_TAG_TYPE };
    _write_type_definition("type", &type_type, file, false);

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        _write_type_definition(def->name, &def->type, file, false);
    }

    fprintf(file, "};\n");
}

void compiler_write_functions(compiler_t *compiler, FILE *file) {

    {
        /* TYPE_TAG_ANY */

        const char *name = compiler->any_type_name;

        fprintf(file, "void %s_cleanup(%s_t *it) {\n",
            name, name);
        fprintf(file, "    if (!it->type || !it->type->cleanup) return;\n");
        fprintf(file, "    /* NOTE: we can safely use it->u.p, because types which have\n");
        fprintf(file, "    a cleanup function are guaranteed to use pointers. */\n");
        fprintf(file, "    it->type->cleanup(it->u.p);\n");
        fprintf(file, "}\n");
        fprintf(file, "void %s_cleanup_voidstar(void *it) { %s_cleanup((%s_t *) it); }\n",
            name, name,
            name);

        fprintf(file, "int %s_parse(%s_t *it, lexer_t *lexer) {\n",
            name, name);
        /* !!! TODO !!! */
        fprintf(file, "}\n");
        fprintf(file, "int %s_parse_voidstar(void *it, lexer_t *lexer) { return %s_parse((%s_t *) it, lexer); }\n",
            name, name,
            name);

        fprintf(file, "int %s_write(%s_t *it, writer_t *writer) {\n",
            name, name);
        /* !!! TODO !!! */
        fprintf(file, "}\n");
        fprintf(file, "int %s_write_voidstar(void *it, writer_t *writer) { return %s_write((%s_t *) it, writer); }\n",
            name, name,
            name);
    }

    {
        /* TYPE_TAG_TYPE */

        const char *name = compiler->type_type_name;

        fprintf(file, "int %s_parse(%s_t *it, lexer_t *lexer) {\n",
            name, name);
        /* !!! TODO !!! */
        /* Ok, unfortunately at the moment we have type_t *it, which we must
        populate... except that each type is represented by a UNIQUE type_t*
        value.
        So we can't just make *it look like the correct type_t.
        So, I hereby propose renaming current struct type to struct type_info,
        and then having a new struct type { int index }, where index is an
        index into extern type_t types[TYPES].
        And then all we need to do is... parse a type name, and look up its
        index by iterating over types[TYPES]. */
        fprintf(file, "}\n");
        fprintf(file, "int %s_parse_voidstar(void *it, lexer_t *lexer) { return %s_parse((%s_t *) it, lexer); }\n",
            name, name, name);

        fprintf(file, "int %s_write(%s_t *it, writer_t *writer) {\n",
            name, name);
        /* !!! TODO !!! */
        /* See above, here we should look up type_info_t *info using it->index
        as an index into types[TYPES], and then we should write info->name. */
        fprintf(file, "}\n");
        fprintf(file, "int %s_write_voidstar(void *it, writer_t *writer) { return %s_write((%s_t *) it, writer); }\n",
            name, name, name);
    }

    ARRAY_FOR_PTR(type_def_t, compiler->defs, def) {
        _write_cleanup_function(def, file);
        _write_parse_function(def, file);
        _write_write_function(def, file);
    }
}
