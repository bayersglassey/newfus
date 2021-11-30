#ifndef _LITERAL_H_
#define _LITERAL_H_

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "array.h"
#include "lexer.h"
#include "stringstore.h"


typedef struct type_literal type_literal_t;

typedef ARRAYOF(type_literal_t) arrayof_inplace_type_literal_t;


enum type_literal_tag {
    TYPE_LITERAL_TAG_INT,
    TYPE_LITERAL_TAG_STRING,
    TYPE_LITERAL_TAG_ARRAY,
    TYPE_LITERAL_TAG_LOCAL_NAME,
    TYPE_LITERAL_TAG_GLOBAL_NAME,
    TYPE_LITERAL_TAG_UNDEFINED,
    TYPE_LITERAL_TAGS
};

struct type_literal {
    int tag; /* enum type_literal_tag */
    union {
        int int_f;
        const char *string_f;
        arrayof_inplace_type_literal_t array_f;
        const char *name_f;
    } u;
};

void type_literal_cleanup(type_literal_t *literal);
int type_literal_parse(type_literal_t *literal, lexer_t *lexer,
    stringstore_t *store);
void type_literal_write(type_literal_t *literal, FILE *file, int depth);


#endif