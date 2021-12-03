#ifndef _LEXER_H_
#define _LEXER_H_

/*
    Lexer usage example:

        stringstore_t store;
        stringstore_init(&store);

        lexer_t lexer;
        lexer_init(&lexer, &store);

        int err = lexer_load(&lexer, "1 2 (3 4) 5", "<filename>");
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
        stringstore_cleanup(&store);

*/

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>




/* Expected from other translation units */
typedef struct stringstore stringstore_t;
typedef struct tokentree tokentree_t;


/* Expected from lexer.c */
typedef struct tokentree_frame tokentree_frame_t;


enum lexer_token_type {
    LEXER_TOKEN_DONE,
    LEXER_TOKEN_INT,
    LEXER_TOKEN_NAME,
    LEXER_TOKEN_OP,
    LEXER_TOKEN_STR,
    LEXER_TOKEN_BLOCKSTR,
    LEXER_TOKEN_OPEN,
    LEXER_TOKEN_CLOSE,
    LEXER_TOKEN_TYPES
};


typedef struct lexer {
    const char *filename;

    /* Should be non-NULL *unless* you only plan to load tokentrees,
    or never plan to use the lexer_get_const_* methods */
    stringstore_t *store;

    int text_len;
    const char *text;

    int token_len;
    const char *token;
    int token_type; /* enum lexer_token_type */

    int pos;
    int row;
    int col;

    /* If positive, represents a series of "(" tokens being returned.
    If negative, represents a series of ")" tokens being returned. */
    int returning_indents;


    /* STACKS: */
    /* (TODO: use array.h instead of implementing these by hand) */

    int indent;
    int indents_size;
    int indents_len;
    int *indents;

    /* NOTE: if lexer->loaded_tokentree != NULL, then we are parsing it
    instead of lexer->text.
    Also, if loaded_tokentree != NULL, then tokentree == NULL represents
    "end of file" (i.e. LEXER_TOKEN_DONE). */
    tokentree_t *loaded_tokentree;
    tokentree_t *tokentree;
    int tokentree_frames_size;
    int tokentree_frames_len;
    tokentree_frame_t *tokentree_frames;
} lexer_t;


void lexer_cleanup(lexer_t *lexer);
void lexer_init(lexer_t *lexer, stringstore_t *store);
void lexer_dump(lexer_t *lexer, FILE *f);
void lexer_info(lexer_t *lexer, FILE *f);
void lexer_err_info(lexer_t *lexer);
int lexer_load(lexer_t *lexer, const char *text,
    const char *filename);
int lexer_load_tokentree(lexer_t *lexer, tokentree_t *tokentree,
    const char *filename);
void lexer_unload(lexer_t *lexer);
bool lexer_loaded(lexer_t *lexer);
int lexer_next(lexer_t *lexer);
bool lexer_done(lexer_t *lexer);
bool lexer_got(lexer_t *lexer, const char *text);
bool lexer_got_name(lexer_t *lexer);
bool lexer_got_op(lexer_t *lexer);
bool lexer_got_str(lexer_t *lexer);
bool lexer_got_int(lexer_t *lexer);
bool lexer_got_open(lexer_t *lexer);
bool lexer_got_close(lexer_t *lexer);
void lexer_show(lexer_t *lexer, FILE *f);
int lexer_get(lexer_t *lexer, const char *text);
int lexer_get_string(lexer_t *lexer, char **string);
int lexer_get_const_string(lexer_t *lexer, const char **string);
int lexer_get_name(lexer_t *lexer, char **name);
int lexer_get_const_name(lexer_t *lexer, const char **name);
int lexer_get_op(lexer_t *lexer, char **op);
int lexer_get_const_op(lexer_t *lexer, const char **op);
int lexer_get_str(lexer_t *lexer, char **s);
int lexer_get_const_str(lexer_t *lexer, const char **s);
int lexer_get_int(lexer_t *lexer, int *i);
int lexer_get_open(lexer_t *lexer);
int lexer_get_close(lexer_t *lexer);
int lexer_unexpected(lexer_t *lexer, const char *expected);
int lexer_parse_silent(lexer_t *lexer);

#endif