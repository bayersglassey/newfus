#ifndef _COMPILER_H_
#define _COMPILER_H_


#include <stddef.h>
#include <stdbool.h>

#include "type.h"
#include "lexer.h"
#include "stringstore.h"



typedef struct compiler {
    ARRAYOF(type_t *) types;

    /* Weakrefs: */
    lexer_t *lexer;
    stringstore_t *stringstore;
} compiler_t;


void compiler_init(compiler_t *compiler, lexer_t *lexer,
    stringstore_t *stringstore);
void compiler_cleanup(compiler_t *compiler);
int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename);

#endif