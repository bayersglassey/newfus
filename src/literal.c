
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "literal.h"
#include "lexer_macros.h"



void type_literal_cleanup(type_literal_t *literal) {
    switch(literal->tag) {
        case TYPE_LITERAL_TAG_ARRAY: {
            ARRAY_FOR(type_literal_t, literal->u.array_f, elem) {
                type_literal_cleanup(elem);
            }
        }
        default: break;
    }
}


int type_literal_parse(type_literal_t *literal, lexer_t *lexer,
    stringstore_t *store
) {
    int err;

    memset(literal, 0, sizeof(*literal));
    literal->tag = TYPE_LITERAL_TAG_UNDEFINED;

    if (GOT_OPEN) {
        literal->tag = TYPE_LITERAL_TAG_ARRAY;
        NEXT
        while (!DONE && !GOT_CLOSE) {
            ARRAY_PUSH(type_literal_t, literal->u.array_f, elem)
            err = type_literal_parse(elem, lexer, store);
            if (err) return err;
        }
        GET_CLOSE
    } else if (GOT("@")) {
        NEXT
        const char *name;
        GET_CONST_NAME(name, store)
        literal->tag = TYPE_LITERAL_TAG_LOCAL_NAME;
        literal->u.name_f = name;
    } else if (GOT("@@")) {
        NEXT
        const char *name;
        GET_CONST_NAME(name, store)
        literal->tag = TYPE_LITERAL_TAG_GLOBAL_NAME;
        literal->u.name_f = name;
    } else if (GOT_INT) {
        int i;
        GET_INT(i)
        literal->tag = TYPE_LITERAL_TAG_INT;
        literal->u.int_f = i;
    } else if (GOT_NAME) {
        const char *string;
        GET_CONST_NAME(string, store)
        literal->tag = TYPE_LITERAL_TAG_STRING;
        literal->u.string_f = string;
    } else if (GOT_STR) {
        const char *string;
        GET_CONST_STR(string, store)
        literal->tag = TYPE_LITERAL_TAG_STRING;
        literal->u.string_f = string;
    } else {
        return UNEXPECTED(
            "one of: integer (e.g. 123, -10), "
            "string (e.g. x, hello, \"world\"), "
            "array (e.g. (1 2 3)), "
            "name reference (e.g. @local_name, @@fully_qualified_name)");
    }
    return 0;
}

void type_literal_write(type_literal_t *literal, FILE *file, int depth) {
    /* If depth < 0, we write arrays inline, with parentheses.
    If depth >= 0, we write arrays using ':', newlines, and indentation. */
    switch(literal->tag) {
        case TYPE_LITERAL_TAG_INT:
            fprintf(file, "%i", literal->u.int_f);
            break;
        case TYPE_LITERAL_TAG_STRING:
            fprintf(file, "%s", literal->u.string_f);
            break;
        case TYPE_LITERAL_TAG_ARRAY: {
            if (depth < 0) fputc('(', file);
            else fputc(':', file);
            ARRAY_FOR(type_literal_t, literal->u.array_f, elem) {
                int new_depth = depth;
                if (new_depth < 0) {
                    if (elem != &literal->u.array_f.elems[0]) fputc(' ', file);
                } else {
                    new_depth++;
                    fputc('\n', file);
                    for (int i = 0; i < new_depth; i++) fputs("    ", file);
                }
                type_literal_write(elem, file, new_depth);
            }
            if (depth < 0) fputc(')', file);
            break;
        }
        case TYPE_LITERAL_TAG_LOCAL_NAME:
            fprintf(file, "@%s", literal->u.name_f);
            break;
        case TYPE_LITERAL_TAG_GLOBAL_NAME:
            fprintf(file, "@@%s", literal->u.name_f);
            break;
        case TYPE_LITERAL_TAG_UNDEFINED:
            /* Caller should guarantee this never happens */
            fprintf(file, "XXX_UNDEFINED_XXX");
            break;
        default:
            /* Caller should guarantee this never happens */
            fprintf(file, "XXX_UNKNOWN_XXX");
            break;
    }
}
