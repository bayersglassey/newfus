#ifndef _LEXER_H_
#define _LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "stringstore.h"

/*
    Lexer usage example:

        lexer_t lexer;
        lexer_init(&lexer);

        int err = lexer_load(&lexer, "1 2 (3 4) 5");
        if (err) return err;

        while (1) {
            if (lexer_done(&lexer)) break;
            printf("Lexed: ");
            lexer_show(&lexer, stdout);
            printf("\n");
            int err = lexer_next(&lexer);
            if (err) return err;
        }

        lexer_cleanup(&lexer);

*/

typedef struct lexer {
    const char *filename;

    int text_len;
    const char *text;

    int token_len;
    const char *token;
    enum {
        LEXER_TOKEN_DONE,
        LEXER_TOKEN_INT,
        LEXER_TOKEN_NAME,
        LEXER_TOKEN_OP,
        LEXER_TOKEN_STR,
        LEXER_TOKEN_BLOCKSTR,
        LEXER_TOKEN_OPEN,
        LEXER_TOKEN_CLOSE,
        LEXER_TOKEN_TYPES
    } token_type;

    int pos;
    int row;
    int col;
    int indent;
    int indents_size;
    int n_indents;
    int *indents;

    /* If positive, represents a series of "(" tokens being returned.
    If negative, represents a series of ")" tokens being returned. */
    int returning_indents;
} lexer_t;


void lexer_cleanup(lexer_t *lexer);
void lexer_init(lexer_t *lexer);
void lexer_dump(lexer_t *lexer, FILE *f);
void lexer_info(lexer_t *lexer, FILE *f);
void lexer_err_info(lexer_t *lexer);
int lexer_load(lexer_t *lexer, const char *text,
    const char *filename);
void lexer_unload(lexer_t *lexer);
bool lexer_loaded(lexer_t *lexer);
int lexer_next(lexer_t *lexer);
bool lexer_done(lexer_t *lexer);
bool lexer_got(lexer_t *lexer, const char *text);
bool lexer_got_name(lexer_t *lexer);
bool lexer_got_str(lexer_t *lexer);
bool lexer_got_int(lexer_t *lexer);
bool lexer_got_open(lexer_t *lexer);
bool lexer_got_close(lexer_t *lexer);
void lexer_show(lexer_t *lexer, FILE *f);
int lexer_get(lexer_t *lexer, const char *text);
int lexer_get_name(lexer_t *lexer, char **name);
int lexer_get_const_name(lexer_t *lexer, stringstore_t *stringstore,
    const char **name);
int lexer_get_str(lexer_t *lexer, char **s);
int lexer_get_int(lexer_t *lexer, int *i);
int lexer_get_open(lexer_t *lexer);
int lexer_get_close(lexer_t *lexer);
int lexer_unexpected(lexer_t *lexer, const char *expected);
int lexer_parse_silent(lexer_t *lexer);

#endif