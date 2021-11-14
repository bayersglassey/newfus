#ifndef _ARRAY_H_
#define _ARRAY_H_

#define DECLARE_ARRAYOF(TYPE) \
typedef struct arrayof_##TYPE { \
    size_t size; \
    size_t len; \
    TYPE##_t *elems; \
} arrayof_##TYPE##_t;

#define DECLARE_PTRARRAYOF(TYPE) \
typedef struct ptrarrayof_##TYPE { \
    size_t size; \
    size_t len; \
    TYPE##_t **elems; \
} ptrarrayof_##TYPE##_t;

#endif