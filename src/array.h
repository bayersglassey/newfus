#ifndef _ARRAY_H_
#define _ARRAY_H_

#define ARRAYOF(TYPE) struct { \
    size_t size; \
    size_t len; \
    TYPE *elems; \
}

#define ARRAY_CLONE(TYPE, A1, A2) \
{ \
    size_t len = (A2).len; \
    size_t size = (A2).size; \
    TYPE *elems = calloc(size, sizeof(*elems)); \
    if (elems == NULL) return 1; \
    for (size_t i = 0; i < len; i++) { \
        elems[i] = (A2).elems[i]; \
    } \
    (A1).len = len; \
    (A1).size = size; \
    (A1).elems = elems; \
}

#define ARRAY_UNHOOK(TYPE, A1, ELEM) \
{ \
    TYPE elem = (ELEM); \
    size_t i1 = (A1).len; \
    for (size_t i = 0; i < i1; i++) { \
        if ((A1).elems[i] == elem) { \
            size_t j1 = (A1).len - 1; \
            for (size_t j = i; j < j1; j++) { \
                (A1).elems[j] = (A1).elems[j + 1]; \
            } \
            (A1).len--; \
        } \
    } \
}

/* Grows array A1's backing array of elements.
Multiplies its size by 2 (or starts it at 8 if size was 0). */
#define ARRAY_GROW(TYPE, A1) \
{ \
    size_t new_size; \
    TYPE *new_elems; \
    if ((A1).size == 0) { \
        new_size = 8; \
        new_elems = malloc(sizeof(*new_elems) * new_size); \
    } else { \
        new_size = (A1).size * 2; \
        new_elems = realloc((A1).elems, \
            sizeof(*new_elems) * new_size); \
    } \
    if (new_elems == NULL) return 1; \
    for (size_t i = (A1).size; i < new_size; i++) { \
        new_elems[i] = (TYPE) {0}; \
    } \
    (A1).elems = new_elems; \
    (A1).size = new_size; \
}

#define ARRAY_PUSH_UNINITIALIZED(TYPE, A1) \
{ \
    if ((A1).len >= (A1).size) ARRAY_GROW(TYPE, A1) \
    (A1).len++; \
}

#define ARRAY_PUSH(TYPE, A1, ELEM) \
{ \
    ARRAY_PUSH_UNINITIALIZED(TYPE, A1) \
    (A1).elems[(A1).len - 1] = (ELEM); \
}

#define ARRAY_PUSH_NEW(TYPE, A1, ELEM_VAR) \
TYPE ELEM_VAR = NULL; \
{ \
    if ((A1).len >= (A1).size) ARRAY_GROW(TYPE, A1) \
    ELEM_VAR = calloc(1, sizeof(*ELEM_VAR)); \
    if (ELEM_VAR == NULL) return 1; \
    (A1).len++; \
    (A1).elems[(A1).len - 1] = ELEM_VAR; \
}

#define ARRAY_FREE(A1, CLEANUP) \
{ \
    for (size_t i = 0; i < (A1).len; i++) { \
        CLEANUP(&(A1).elems[i]); \
    } \
    free((A1).elems); \
    (A1).len = 0; \
    (A1).size = 0; \
}

#define ARRAY_FREE_PTR(A1, CLEANUP) \
{ \
    for (size_t i = 0; i < (A1).len; i++) { \
        CLEANUP((A1).elems[i]); \
        free((A1).elems[i]); \
    } \
    free((A1).elems); \
    (A1).len = 0; \
    (A1).size = 0; \
}

#define ARRAY_POP(A1, CLEANUP) \
    if ((A1).len <= 0) return 2; \
    (A1).len--; \
    elem_cleanup((A1).elems[(A1).len]);

#define ARRAY_POP_PTR(A1, CLEANUP) \
    if ((A1).len <= 0) return 2; \
    (A1).len--; \
    elem_cleanup((A1).elems[(A1).len]); \
    free((A1).elems[(A1).len]);

#endif