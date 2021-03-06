#ifndef _TOKENTREE_H_
#define _TOKENTREE_H_

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "array.h"


/* Expected from other translation units */
typedef struct lexer lexer_t;
typedef struct writer writer_t;


typedef struct tokentree tokentree_t;

typedef ARRAYOF(tokentree_t) arrayof_inplace_tokentree_t;


enum tokentree_tag {
    TOKENTREE_TAG_INT,
    TOKENTREE_TAG_NAME,
    TOKENTREE_TAG_OP,
    TOKENTREE_TAG_STR,
    TOKENTREE_TAG_ARR,
    TOKENTREE_TAG_UNDEFINED,
    TOKENTREE_TAGS
};

static bool tokentree_tag_is_string(int tag) {
    return
        tag == TOKENTREE_TAG_NAME ||
        tag == TOKENTREE_TAG_OP ||
        tag == TOKENTREE_TAG_STR;
}

struct tokentree {
    int tag; /* enum tokentree_tag */
    union {
        int int_f;
        const char *string_f;
        arrayof_inplace_tokentree_t array_f;
    } u;
};

void tokentree_cleanup(tokentree_t *tokentree);
int tokentree_parse(tokentree_t *tokentree, lexer_t *lexer);
int tokentree_write(tokentree_t *tokentree, writer_t *writer);


#endif