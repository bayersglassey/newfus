
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "tokentree.h"
#include "lexer.h"
#include "lexer_macros.h"



void tokentree_cleanup(tokentree_t *tokentree) {
    switch(tokentree->tag) {
        case TOKENTREE_TAG_ARR: {
            ARRAY_FOR(tokentree_t, tokentree->u.array_f, elem) {
                tokentree_cleanup(elem);
            }
            free(tokentree->u.array_f.elems);
        }
        default: break;
    }
}


int tokentree_parse(tokentree_t *tokentree, lexer_t *lexer) {
    int err;

    memset(tokentree, 0, sizeof(*tokentree));
    tokentree->tag = TOKENTREE_TAG_UNDEFINED;

    if (GOT_OPEN) {
        tokentree->tag = TOKENTREE_TAG_ARR;
        NEXT
        while (!DONE && !GOT_CLOSE) {
            ARRAY_PUSH(tokentree_t, tokentree->u.array_f, elem)
            err = tokentree_parse(elem, lexer);
            if (err) return err;
        }
        GET_CLOSE
    } else if (GOT_INT) {
        int i;
        GET_INT(i)
        tokentree->tag = TOKENTREE_TAG_INT;
        tokentree->u.int_f = i;
    } else if (GOT_NAME) {
        const char *string;
        GET_CONST_NAME(string)
        tokentree->tag = TOKENTREE_TAG_NAME;
        tokentree->u.string_f = string;
    } else if (GOT_OP) {
        const char *string;
        GET_CONST_OP(string)
        tokentree->tag = TOKENTREE_TAG_OP;
        tokentree->u.string_f = string;
    } else if (GOT_STR) {
        const char *string;
        GET_CONST_STR(string)
        tokentree->tag = TOKENTREE_TAG_STR;
        tokentree->u.string_f = string;
    } else {
        return UNEXPECTED(
            "one of: INT (e.g. 123, -10), "
            "NAME (e.g. x, x2, hello_world, SomeName), "
            "OP (e.g. +, --, =>), "
            "STR (e.g. \"hello world\", \"a \\\"quoted\\\" thing\", \"two\\nlines\"), "
            "ARR (e.g. (1 2 3))");
    }
    return 0;
}

static void _str_putc(FILE *f, char c){
    if(c == '\n'){
        fputs("\\n", f);
    }else if(c == '\"' || c == '\\'){
        fputc('\\', f);
        fputc(c, f);
    }else{
        fputc(c, f);
    }
}

static void _str_write(FILE *f, const char *s){
    fputc('\"', f);
    char c;
    while(c = *s, c != '\0'){
        _str_putc(f, c);
        s++;
    }
    fputc('\"', f);
}

void tokentree_write(tokentree_t *tokentree, FILE *file, int depth) {
    /* If depth < 0, we write arrays inline, with parentheses.
    If depth >= 0, we write arrays using ':', newlines, and indentation. */
    switch(tokentree->tag) {
        case TOKENTREE_TAG_INT:
            fprintf(file, "%i", tokentree->u.int_f);
            break;
        case TOKENTREE_TAG_NAME:
        case TOKENTREE_TAG_OP:
            fputs(tokentree->u.string_f, file);
            break;
        case TOKENTREE_TAG_STR:
            _str_write(file, tokentree->u.string_f);
            break;
        case TOKENTREE_TAG_ARR: {
            if (depth < 0) fputc('(', file);
            else fputc(':', file);
            ARRAY_FOR(tokentree_t, tokentree->u.array_f, elem) {
                int new_depth = depth;
                if (new_depth < 0) {
                    if (elem != &tokentree->u.array_f.elems[0]) fputc(' ', file);
                } else {
                    new_depth++;
                    fputc('\n', file);
                    for (int i = 0; i < new_depth; i++) fputs("    ", file);
                }
                tokentree_write(elem, file, new_depth);
            }
            if (depth < 0) fputc(')', file);
            break;
        }
        case TOKENTREE_TAG_UNDEFINED:
            /* Caller should guarantee this never happens */
            fprintf(file, "XXX_UNDEFINED_XXX");
            break;
        default:
            /* Caller should guarantee this never happens */
            fprintf(file, "XXX_UNKNOWN_XXX");
            break;
    }
}
