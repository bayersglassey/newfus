

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"


void compiler_init(compiler_t *compiler, lexer_t *lexer,
    stringstore_t *store
) {
    memset(compiler, 0, sizeof(*compiler));
    compiler->any_type_name = "any";
    compiler->any_type_name_upper = "ANY";
    compiler->type_type_name = "type";
    compiler->type_type_name_upper = "TYPE";
    compiler->types_name = "types";
    compiler->types_name_upper = "TYPES";
    compiler->lexer = lexer;
    compiler->store = store;
}

void compiler_cleanup(compiler_t *compiler) {
    ARRAY_FREE_PTR(compiler->defs, type_def_cleanup)
    ARRAY_FREE_PTR(compiler->bindings, compiler_binding_cleanup)
}

void compiler_dump(compiler_t *compiler, FILE *file) {
    fprintf(file, "Compiler %p:\n", compiler);

    fprintf(file, "Bindings:\n");
    for (int i = 0; i < compiler->bindings.len; i++) {
        compiler_binding_t *binding = compiler->bindings.elems[i];
        fprintf(file, "  %i/%zu: %s -> %s\n", i, compiler->bindings.len,
            binding->name, binding->def->name);
    }

    fprintf(file, "Defs:\n");
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        fprintf(file, "  %i/%zu: %s (%s)", i, compiler->defs.len, def->name,
            type_tag_string(def->type.tag));
        switch (def->type.tag) {
            case TYPE_TAG_ARRAY: {
                type_t *subtype = &def->type.u.array_f.subtype_ref->type;
                fprintf(stderr, " -> (%s)", type_tag_string(subtype->tag));
                type_def_t *def = type_get_def(subtype);
                if (def) fprintf(stderr, " -> %s", def->name);
                fputc('\n', stderr);
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                fputc('\n', stderr);
                arrayof_inplace_type_field_t *fields = &def->type.u.struct_f.fields;
                for (int i = 0; i < fields->len; i++) {
                    type_field_t *field = &fields->elems[i];
                    fprintf(stderr, "    %s (%s)", field->name,
                        type_tag_string(field->ref.type.tag));
                    type_def_t *def = type_get_def(&field->ref.type);
                    if (def) fprintf(stderr, " -> %s", def->name);
                    fputc('\n', stderr);
                }
                break;
            }
            case TYPE_TAG_ALIAS: {
                fprintf(stderr, " -> %s\n", def->type.u.alias_f.def->name);
                break;
            }
            case TYPE_TAG_FUNC: {
                fprintf(stderr, " -> %s\n", def->type.u.func_f.def->name);
                break;
            }
            default: {
                fputc('\n', stderr);
                break;
            }
        }
    }
}

void compiler_debug_info(compiler_t *compiler) {
    lexer_info(compiler->lexer, stderr);
}

void compiler_binding_cleanup(compiler_binding_t *binding) {
    /* Nuthin */
}



int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    err = lexer_load(lexer, buffer, filename);
    if (err) return err;

    err = compiler_parse_defs(compiler);
    if (err) {
        lexer_info(lexer, stderr);
        fprintf(stderr, "Failed to parse\n");
        compiler_dump(compiler, stderr);
        return err;
    }

    if (!compiler_validate(compiler)) return 2;

    if (compiler->defs.len) {
        /* Sort compiler->defs such that arrays/structs/unions come after any
        defs they have an inplace reference to. */
        err = compiler_sort_inplace_refs(compiler);
        if (err) return err;

        /* Sort compiler->defs such that the compiled C typedefs come after
        any other C typedefs they refer to */
        err = compiler_sort_typedefs(compiler);
        if (err) return err;
    }

    lexer_unload(lexer);
    return 0;
}
