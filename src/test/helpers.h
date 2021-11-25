#ifndef _TEST_HELPERS_H_
#define _TEST_HELPERS_H_

/* TODO: decide where/how these things are to be output automatically
(or do we just provide "runtime" .h and .c files?..) */
typedef struct any any_t;
struct any {
    int dummy;
};
void any_cleanup(any_t *it);





typedef struct util_math_complex util_math_complex_t;
typedef struct util_math_real util_math_real_t;

struct util_math_complex {
    int dummy;
};

struct util_math_real {
    int dummy;
};

void util_math_complex_cleanup(util_math_complex_t *it);
void util_math_real_cleanup(util_math_real_t *it);

#endif