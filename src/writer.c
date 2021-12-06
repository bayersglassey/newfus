
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "writer.h"



void writer_cleanup(writer_t *writer) {
    /* Nothing to do */
}

void writer_init(writer_t *writer, FILE *file) {
    memset(writer, 0, sizeof(*writer));
    writer->file = file;
    writer->indent_string = "    ";
}

void writer_reset(writer_t *writer) {
    writer->depth = 0;
    writer->needs_space = false;
    writer->needs_newline = false;
}



#define _FPUTC(CHAR) if (fputc(CHAR, writer->file) == EOF) return 1;
#define _FPUTS(STRING) if (fputs(STRING, writer->file) == EOF) return 1;
#define _FPRINTF(...) if (fprintf(writer->file, __VA_ARGS__) == EOF) return 1;


static int writer_write_separator(writer_t *writer) {
    if (writer->needs_newline) {
        _FPUTC('\n')
        for (int i = 0; i < writer->add_spaces; i++) _FPUTC(' ')
        for (int i = 0; i < writer->depth; i++) _FPUTS(writer->indent_string)
        writer->needs_newline = false;
        writer->needs_space = false;
    } else if (writer->needs_space) {
        _FPUTC(' ')
        writer->needs_space = false;
    }
    return 0;
}

int writer_write_name(writer_t *writer, const char *name) {
    int err = writer_write_separator(writer);
    if (err) return err;
    _FPUTS(name)
    writer->needs_space = true;
    return 0;
}

int writer_write_op(writer_t *writer, const char *op) {
    int err = writer_write_separator(writer);
    if (err) return err;
    _FPUTS(op)
    writer->needs_space = true;
    return 0;
}

int writer_write_str(writer_t *writer, const char *s) {
    int err = writer_write_separator(writer);
    if (err) return err;
    _FPUTC('"')

    char c;
    while(c = *s, c != '\0'){
        if(c == '\n'){
            _FPUTS("\\n");
        }else if(c == '"' || c == '\\'){
            _FPUTC('\\');
            _FPUTC(c);
        }else{
            _FPUTC(c);
        }
        s++;
    }

    _FPUTC('"')
    writer->needs_space = true;
    return 0;
}

int writer_write_blockstr(writer_t *writer, const char *s) {
    /* Blockstrings require newlines, so fall back to writing a regular
    string with quotes "..." if we're writing everything on one line */
    if (writer->oneline) return writer_write_str(writer, s);

    /* MAYBE TODO: check whether s contains newlines, and if so, fall
    back to writer_write_str?..
    Or do we just trust caller to provide us with a "correct" s */

    int err = writer_write_separator(writer);
    if (err) return err;
    _FPUTS(";;")
    _FPUTS(s)
    writer->needs_newline = true;
    return 0;
}

int writer_write_int(writer_t *writer, int i) {
    int err = writer_write_separator(writer);
    if (err) return err;
    _FPRINTF("%i", i)
    writer->needs_space = true;
    return 0;
}

int writer_write_open(writer_t *writer) {
    writer->depth++;
    if (writer->oneline) {
        _FPUTC('(')
        writer->needs_space = false;
    } else {
        _FPUTC(':')
        writer->needs_newline = true;
    }
    return 0;
}

int writer_write_close(writer_t *writer) {
    if (writer->depth == 0) {
        fprintf(stderr, "Tried to write_close when depth == 0\n");
        return 2;
    }
    writer->depth--;
    if (writer->oneline) {
        _FPUTC(')')
        writer->needs_space = true;
    } else {
        writer->needs_newline = true;
    }
    return 0;
}
