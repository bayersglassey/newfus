
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "tokentree.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "writer.h"



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

int tokentree_write(tokentree_t *tokentree, writer_t *writer) {
    int err;
    switch(tokentree->tag) {
        case TOKENTREE_TAG_INT:
            return writer_write_int(writer, tokentree->u.int_f);
        case TOKENTREE_TAG_NAME:
            return writer_write_name(writer, tokentree->u.string_f);
        case TOKENTREE_TAG_OP:
            return writer_write_op(writer, tokentree->u.string_f);
        case TOKENTREE_TAG_STR:
            return writer_write_str(writer, tokentree->u.string_f);
        case TOKENTREE_TAG_ARR: {
            err = writer_write_open(writer);
            if (err) return err;
            ARRAY_FOR(tokentree_t, tokentree->u.array_f, elem) {
                err = tokentree_write(elem, writer);
                if (err) return err;
            }
            return writer_write_close(writer);
        }
        case TOKENTREE_TAG_UNDEFINED:
            fprintf(stderr, "%s: Encountered UNDEFINED tokentree node\n",
                __func__);
            return 2;
        default:
            fprintf(stderr, "%s: Unrecognized tokentree tag: %i\n",
                __func__, tokentree->tag);
            return 2;
    }
}
