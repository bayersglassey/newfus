#ifndef _COMPILER_H_
#define _COMPILER_H_


#include <stddef.h>
#include <stdbool.h>

#include "type.h"
#include "lexer.h"
#include "stringstore.h"


DECLARE_TYPE(compiler)
DECLARE_TYPE(compiler_binding)


struct compiler {
    bool debug;

    const char *package_name;
    ARRAYOF(compiler_binding_t *) bindings;
    ARRAYOF(type_def_t *) defs;

    const char *any_type_name; /* e.g. "any" */
    const char *any_type_name_upper; /* e.g. "ANY" */
    const char *type_type_name; /* e.g. "type" */
    const char *type_type_name_upper; /* e.g. "TYPE" */
    const char *types_name; /* e.g. "types" */
    const char *types_name_upper; /* e.g. "TYPES" */

    /* can_rebind: whether bindings can be redefined, e.g. with "from" */
    bool can_rebind;

    /* can_redef: whether defs can be redefined, e.g. with "typedef" */
    bool can_redef;

    /* Weakrefs: */
    lexer_t *lexer;
    stringstore_t *store;
};

/* Binds a non-fully-qualified name to a type_def, for use with syntax
"from PACKAGE: NAME" */
struct compiler_binding {
    const char *name;

    /* Weakrefs: */
    type_def_t *def;
};


void compiler_init(compiler_t *compiler, lexer_t *lexer,
    stringstore_t *store);
void compiler_dump(compiler_t *compiler, FILE *file);
void compiler_debug_info(compiler_t *compiler);
int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename);
int compiler_parse_defs(compiler_t *compiler);
bool compiler_validate(compiler_t *compiler);
int compiler_sort_inplace_refs(compiler_t *compiler);
int compiler_sort_typedefs(compiler_t *compiler);
void compiler_write_hfile(compiler_t *compiler, FILE *file);
void compiler_write_cfile(compiler_t *compiler, FILE *file);
void compiler_write_typedefs(compiler_t *compiler, FILE *file);
void compiler_write_enums(compiler_t *compiler, FILE *file);
void compiler_write_structs(compiler_t *compiler, FILE *file);
void compiler_write_type_declarations(compiler_t *compiler, FILE *file);
void compiler_write_prototypes(compiler_t *compiler, FILE *file);
void compiler_write_type_definitions(compiler_t *compiler, FILE *file);
void compiler_write_functions(compiler_t *compiler, FILE *file);

#endif