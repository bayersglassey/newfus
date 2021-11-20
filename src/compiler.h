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
    const char *package_name;
    ARRAYOF(compiler_binding_t *) bindings;
    ARRAYOF(type_def_t *) defs;

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
int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename);

#endif