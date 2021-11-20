

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler.h"
#include "../file_utils.h"



static void print_usage(FILE *file) {
    fprintf(file,
        "Options: [OPTION ...] [--] [FILE ...]\n"
        "  -h  --help   Print this message & exit\n"
        "  -D  --debug  Dump debug information to stderr\n"
    );
}


int main(int n_args, char **args) {
    int err;

    bool debug = false;

    int arg_i = 1;
    for (; arg_i < n_args; arg_i++) {
        const char *arg = args[arg_i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_usage(stderr);
            return 0;
        } else if (!strcmp(arg, "-D") || !strcmp(arg, "--debug")) {
            debug = true;
        } else if (!strcmp(arg, "--")) {
            arg_i++;
            break;
        } else {
            break;
        }
    }

    {
        lexer_t lexer;
        stringstore_t stringstore;
        compiler_t compiler;

        lexer_init(&lexer);
        stringstore_init(&stringstore);
        compiler_init(&compiler, &lexer, &stringstore);

        for (; arg_i < n_args; arg_i++) {
            const char *filename = args[arg_i];
            fprintf(stderr, "Compiling: %s\n", filename);

            char *buffer = load_file(filename);
            if (!buffer) return 1;

            err = compiler_compile(&compiler, buffer, filename);
            if (err) return err;

            free(buffer);
            fprintf(stderr, "...done compiling: %s\n", filename);
        }

        if (debug) {
            stringstore_dump(&stringstore, stderr);
            compiler_dump(&compiler, stderr);
        }

        compiler_cleanup(&compiler);
        lexer_cleanup(&lexer);
        stringstore_cleanup(&stringstore);
    }

    fprintf(stderr, "OK!\n");
    return 0;
}
