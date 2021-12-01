

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../tokentree.h"
#include "../file_utils.h"
#include "../lexer_macros.h"


bool output_inline = false;


static void print_usage(FILE *file) {
    fprintf(file,
        "Usage: tokentree [OPTION ...] [--] [FILE ...]\n"
        "To read stdin, use the filename \"-\".\n"
        "Options:\n"
        "  -h  --help            Print this message and exit\n"
        "  -i  --inline          Output tokentree \"inline\" as opposed to indented\n"
    );
}


static int parse_buffer(const char *buffer, const char *filename,
    stringstore_t *store
) {
    int err;

    lexer_t _lexer, *lexer=&_lexer;
    lexer_init(lexer);

    LOAD(buffer, filename)

    while (!DONE) {
        tokentree_t tokentree;
        err = tokentree_parse(&tokentree, lexer, store);
        if (err) return err;

        int depth = output_inline? -1: 0;
        tokentree_write(&tokentree, stdout, depth);
        fputc('\n', stdout);
    }

    lexer_cleanup(lexer);
    return 0;
}


int main(int n_args, char **args) {
    int err;

    int arg_i = 1;
    for (; arg_i < n_args; arg_i++) {
        char *arg = args[arg_i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_usage(stdout);
            return 0;
        } else if (!strcmp(arg, "-i") || !strcmp(arg, "--inline")) {
            output_inline = true;
        } else if (!strcmp(arg, "--")) {
            arg_i++;
            break;
        } else {
            break;
        }
    }

    stringstore_t store;
    stringstore_init(&store);

    for (; arg_i < n_args; arg_i++) {
        char *filename = args[arg_i];
        char *buffer;
        if (!strcmp(filename, "-")) {
            filename = "<stdin>";
            buffer = read_stream(stdin, filename);
            if (!buffer) return 1;
        } else {
            buffer = load_file(filename);
            if (!buffer) return 1;
        }

        err = parse_buffer(buffer, filename, &store);
        if (err) return err;

        free(buffer);
    }

    stringstore_cleanup(&store);
    return 0;
}
