

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"


static int parse_type(compiler_t *compiler);
static int parse_type_ref(compiler_t *compiler);
static int parse_type_field(compiler_t *compiler);



void compiler_init(compiler_t *compiler, lexer_t *lexer,
    stringstore_t *stringstore
) {
    memset(compiler, 0, sizeof(*compiler));
    compiler->lexer = lexer;
    compiler->stringstore = stringstore;
}

void compiler_cleanup(compiler_t *compiler) {
    ARRAY_FREE_PTR(compiler->types, type_cleanup)
}



static int parse_type_ref(compiler_t *compiler) {
    int err;
    lexer_t *lexer = compiler->lexer;

    while (lexer_got(lexer, "inline") || lexer_got(lexer, "weakref")) {
        err = lexer_next(lexer);
        if (err) return err;
    }

    if (lexer_got(lexer, "@")) {
        err = lexer_next(lexer);
        if (err) return err;

        const char *name;
        err = lexer_get_const_name(lexer, compiler->stringstore, &name);
        if (err) return err;
    } else {
        err = parse_type(compiler);
        if (err) return err;
    }

    return 0;
}

static int parse_type_field(compiler_t *compiler) {
    int err;
    lexer_t *lexer = compiler->lexer;

    const char *name;
    err = lexer_get_const_name(lexer, compiler->stringstore, &name);
    if (err) return err;
    err = lexer_get(lexer, "(");
    if (err) return err;
    err = parse_type_ref(compiler);
    if (err) return err;
    err = lexer_get(lexer, ")");
    if (err) return err;

    return 0;
}

static int parse_type(compiler_t *compiler) {
    int err;
    lexer_t *lexer = compiler->lexer;

    if (lexer_got(lexer, "void")) {
        err = lexer_next(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "any")) {
        err = lexer_next(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "int")) {
        err = lexer_next(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "sym")) {
        err = lexer_next(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "bool")) {
        err = lexer_next(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "byte")) {
        err = lexer_next(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "arrayof")) {
        err = lexer_next(lexer);
        if (err) return err;
        err = parse_type_ref(compiler);
        if (err) return err;
    } else if (lexer_got(lexer, "struct") || lexer_got(lexer, "union")) {
        err = lexer_next(lexer);
        if (err) return err;
        err = lexer_get(lexer, "(");
        if (err) return err;
        while (!lexer_got(lexer, ")")) {
            err = parse_type_field(compiler);
            if (err) return err;
        }
        err = lexer_next(lexer);
        if (err) return err;
    } else {
        return lexer_unexpected(lexer,
            "void any int sym bool byte array struct union");
    }

    return 0;
}

int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    err = lexer_load(lexer, buffer, filename);
    if (err) return err;

    while (!lexer_done(lexer)) {
        err = lexer_get(lexer, "typedef");
        if (err) return err;

        const char *name;
        err = lexer_get_const_name(lexer, compiler->stringstore, &name);
        if (err) return err;

        err = parse_type(compiler);
        if (err) return err;
    }

    lexer_unload(lexer);
    return 0;
}
