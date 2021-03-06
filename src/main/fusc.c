

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler.h"
#include "../file_utils.h"
#include "../stringstore.h"
#include "../lexer.h"


bool debug = false;
bool dump = false;
bool write_typedefs = false;
bool write_enums = false;
bool write_structs = false;
bool write_type_decls = false;
bool write_protos = false;
bool write_type_defns = false;
bool write_functions = false;
bool write_hfile = false;
bool write_cfile = false;
bool write_main = false;


static void print_usage(FILE *file) {
    fprintf(file,
        "Options: [OPTION ...] [--] [FILE ...]\n"
        "  -h  --help        Print this message & exit\n"
        "  -D  --debug       Compiler dumps debug information to stderr during compilation\n"
        "  -d  --dump        Dump debug information to stderr after compilation\n"
        "  -a  --all         Write all compiled C code to stdout\n"
        "      --hfile       Write compiled .h file to stdout\n"
        "      --cfile       Write compiled .c file to stdout\n"
        "      --main        Write a dummy main function to stdout\n"
        "      --typedefs    Write compiled typedefs to stdout\n"
        "      --enums       Write compiled enums to stdout\n"
        "      --structs     Write compiled C structs (of fus arrays/structs/unions) to stdout\n"
        "      --type_decls  Write compiled type declarations to stdout\n"
        "      --protos      Write compiled function prototypes to stdout\n"
        "      --type_defns  Write compiled type definitions to stdout\n"
        "      --functions   Write compiled function definitions to stdout\n"
    );
}


int _compile(compiler_t *compiler, int n_filenames, char **filenames) {
    int err;

    for (int i = 0; i < n_filenames; i++) {
        const char *filename = filenames[i];
        fprintf(stderr, "Compiling: %s\n", filename);

        char *buffer = load_file(filename);
        if (!buffer) return 1;

        err = compiler_compile(compiler, buffer, filename);
        if (err) return err;

        free(buffer);
        fprintf(stderr, "...done compiling: %s\n", filename);
    }

    if (dump) {
        stringstore_dump(compiler->store, stderr);
        compiler_dump(compiler, stderr);
    }

    if (write_typedefs) compiler_write_typedefs(compiler, stdout);
    if (write_enums) compiler_write_enums(compiler, stdout);
    if (write_structs) compiler_write_structs(compiler, stdout);
    if (write_type_decls) compiler_write_type_declarations(compiler, stdout);
    if (write_protos) compiler_write_prototypes(compiler, stdout);
    if (write_type_defns) compiler_write_type_definitions(compiler, stdout);
    if (write_functions) compiler_write_functions(compiler, stdout);
    if (write_hfile) compiler_write_hfile(compiler, stdout);
    if (write_cfile) compiler_write_cfile(compiler, stdout);
    if (write_main) {
        fputc('\n', stdout);
        fprintf(stdout, "/* Dummy main */\n");
        fprintf(stdout, "int main(int n_args, char **args) { return 0; }\n");
    }

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
        } else if (!strcmp(arg, "-D") || !strcmp(arg, "--debug")) {
            debug = true;
        } else if (!strcmp(arg, "-d") || !strcmp(arg, "--dump")) {
            dump = true;
        } else if (!strcmp(arg, "-a") || !strcmp(arg, "--all")) {
            write_hfile = true;
            write_cfile = true;
            write_main = true;
        } else if (!strcmp(arg, "--hfile")) {
            write_hfile = true;
        } else if (!strcmp(arg, "--cfile")) {
            write_cfile = true;
        } else if (!strcmp(arg, "--main")) {
            write_main = true;
        } else if (!strcmp(arg, "--typedefs")) {
            write_typedefs = true;
        } else if (!strcmp(arg, "--enums")) {
            write_enums = true;
        } else if (!strcmp(arg, "--structs")) {
            write_structs = true;
        } else if (!strcmp(arg, "--type_decls")) {
            write_type_decls = true;
        } else if (!strcmp(arg, "--protos")) {
            write_protos = true;
        } else if (!strcmp(arg, "--type_defns")) {
            write_type_defns = true;
        } else if (!strcmp(arg, "--functions")) {
            write_functions = true;
        } else if (!strcmp(arg, "--")) {
            arg_i++;
            break;
        } else {
            break;
        }
    }

    {
        stringstore_t store;
        lexer_t lexer;
        compiler_t compiler;

        stringstore_init(&store);
        lexer_init(&lexer, &store);
        compiler_init(&compiler, &lexer, &store);

        compiler.debug = debug;

        err = _compile(&compiler, n_args - arg_i, args + arg_i);
        if (err) {
            fprintf(stderr, "FAILED! Exiting with code: %i\n", err);
            return err;
        }

        compiler_cleanup(&compiler);
        lexer_cleanup(&lexer);
        stringstore_cleanup(&store);
    }

    fprintf(stderr, "OK!\n");
    return 0;
}
