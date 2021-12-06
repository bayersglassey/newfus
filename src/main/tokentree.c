

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../tokentree.h"
#include "../file_utils.h"
#include "../stringstore.h"
#include "../lexer.h"
#include "../lexer_macros.h"
#include "../writer.h"


bool output_oneline = false;
bool reparse = false;


static void print_usage(FILE *file) {
    fprintf(file,
        "Usage: tokentree [OPTION ...] [--] [FILE ...]\n"
        "To read stdin, use the filename \"-\".\n"
        "Options:\n"
        "  -h  --help            Print this message and exit\n"
        "  -i  --oneline         Output tokentree \"oneline\" as opposed to indented\n"
        "  -r  --reparse         Parse the parsed tokentree\n"
        "                        (for testing lexer_load_tokentree)\n"
    );
}


static int parse_buffer(const char *buffer, const char *filename,
    stringstore_t *store
) {
    int err;

    lexer_t _lexer, *lexer=&_lexer;
    lexer_init(lexer, store);

    writer_t _writer, *writer=&_writer;
    writer_init(writer, stdout);
    writer->oneline = output_oneline;

    err = lexer_load(lexer, buffer, filename);
    if (err) return err;

    while (!lexer_done(lexer)) {
        tokentree_t tokentree;
        err = tokentree_parse(&tokentree, lexer);
        if (err) return err;

        if (reparse) {
            /* Re-parse the tokentree from itself, to test
            lexer_load_tokentree. */

            lexer_t _lexer2, *lexer2=&_lexer2;
            lexer_init(lexer2, store);

            err = lexer_load_tokentree(lexer2, &tokentree, filename);
            if (err) return err;

            tokentree_t tokentree2;
            err = tokentree_parse(&tokentree2, lexer2);
            if (err) return err;

            lexer_cleanup(lexer2);

            /* Replace the original tokentree (which was parsed from the
            text buffer) with the new one (which was parsed from the old
            one) */
            tokentree_cleanup(&tokentree);
            tokentree = tokentree2;
        }

        writer_reset(writer);
        err = tokentree_write(&tokentree, writer);
        if (err) return err;
        fputc('\n', stdout);

        tokentree_cleanup(&tokentree);
    }

    lexer_cleanup(lexer);
    writer_cleanup(writer);
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
        } else if (!strcmp(arg, "-i") || !strcmp(arg, "--oneline")) {
            output_oneline = true;
        } else if (!strcmp(arg, "-r") || !strcmp(arg, "--reparse")) {
            reparse = true;
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
