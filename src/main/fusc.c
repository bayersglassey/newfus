

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../type.h"
#include "../value.h"
#include "../file_utils.h"
#include "../lexer.h"


static int parse_type(lexer_t *lexer);
static int parse_type_ref(lexer_t *lexer);
static int parse_type_field(lexer_t *lexer);




static void print_usage(FILE *file) {
    fprintf(file,
        "Options: [OPTION ...] [--] [FILE ...]\n"
        "  -h  --help   Print this message & exit\n"
    );
}

static int parse_type_ref(lexer_t *lexer) {
    int err;

    while (lexer_got(lexer, "inline") || lexer_got(lexer, "weakref")) {
        err = lexer_next(lexer);
        if (err) return err;
    }

    if (lexer_got(lexer, "@")) {
        err = lexer_next(lexer);
        if (err) return err;

        char *name;
        err = lexer_get_name(lexer, &name);
        if (err) return err;
        free(name);
    } else {
        err = parse_type(lexer);
        if (err) return err;
    }

    return 0;
}

static int parse_type_field(lexer_t *lexer) {
    int err;

    char *name;
    err = lexer_get_name(lexer, &name);
    if (err) return err;
    err = lexer_get(lexer, "(");
    if (err) return err;
    err = parse_type_ref(lexer);
    if (err) return err;
    err = lexer_get(lexer, ")");
    if (err) return err;

    return 0;
}

static int parse_type(lexer_t *lexer) {
    int err;

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
        err = parse_type_ref(lexer);
        if (err) return err;
    } else if (lexer_got(lexer, "struct") || lexer_got(lexer, "union")) {
        err = lexer_next(lexer);
        if (err) return err;
        err = lexer_get(lexer, "(");
        if (err) return err;
        while (!lexer_got(lexer, ")")) {
            err = parse_type_field(lexer);
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

static int compile_buffer(const char *filename, const char *buffer) {
    int err;

    lexer_t _lexer, *lexer=&_lexer;
    err = lexer_init(lexer, buffer, filename);
    if (err) return err;

    while (!lexer_done(lexer)) {
        err = lexer_get(lexer, "typedef");
        if (err) return err;

        char *name;
        err = lexer_get_name(lexer, &name);
        if (err) return err;

        err = parse_type(lexer);
        if (err) return err;

        free(name);
    }

    lexer_cleanup(lexer);
    return 0;
}

int main(int n_args, char **args) {
    int err;

    int arg_i = 1;
    for (; arg_i < n_args; arg_i++) {
        const char *arg = args[arg_i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_usage(stderr);
            return 0;
        } else if (!strcmp(arg, "--")) {
            arg_i++;
            break;
        } else {
            break;
        }
    }
    for (; arg_i < n_args; arg_i++) {
        const char *filename = args[arg_i];
        fprintf(stderr, "Compiling: %s\n", filename);
        char *buffer = load_file(filename);
        err = compile_buffer(filename, buffer);
        if (err) return err;
        free(buffer);
        fprintf(stderr, "...done compiling: %s\n", filename);
    }

    return 0;
}
