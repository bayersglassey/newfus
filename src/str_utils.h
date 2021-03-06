#ifndef _STR_UTILS_H_
#define _STR_UTILS_H_


#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stringstore.h"


static char *_strdup(const char *s1){
    size_t s1_len = strlen(s1);
    char *s2 = malloc(s1_len + 1);
    if(s2 == NULL)return NULL;
    strcpy(s2, s1);
    return s2;
}

static bool _streq(const char *s1, const char *s2) {
    return s1 == s2 || (s1 && s2 && !strcmp(s1, s2));
}

static void _strtolower(char *s) {
    size_t len = strlen(s);
    for (int i = 0; i < len; i++) {
        s[i] = tolower(s[i]);
    }
}

static void _strtoupper(char *s) {
    size_t len = strlen(s);
    for (int i = 0; i < len; i++) {
        s[i] = toupper(s[i]);
    }
}

static char *_strjoin2(const char *s1, const char *s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t len = len1 + len2;
    char *s = malloc(len + 1);
    if (!s) return NULL;
    strcpy(s, s1);
    strcpy(s + len1, s2);
    s[len] = '\0';
    return s;
}

static char *_strjoin3(const char *s1, const char *s2, const char *s3) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t len3 = strlen(s3);
    size_t len = len1 + len2 + len3;
    char *s = malloc(len + 1);
    if (!s) return NULL;
    strcpy(s, s1);
    strcpy(s + len1, s2);
    strcpy(s + len1 + len2, s3);
    s[len] = '\0';
    return s;
}

static char *_strjoin4(const char *s1, const char *s2, const char *s3,
    const char *s4
) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t len3 = strlen(s3);
    size_t len4 = strlen(s4);
    size_t len = len1 + len2 + len3 + len4;
    char *s = malloc(len + 1);
    if (!s) return NULL;
    strcpy(s, s1);
    strcpy(s + len1, s2);
    strcpy(s + len1 + len2, s3);
    strcpy(s + len1 + len2 + len3, s4);
    s[len] = '\0';
    return s;
}


static const char *_const_strjoin2(stringstore_t *stringstore,
    const char *s1, const char *s2
) {
    char *s = _strjoin2(s1, s2);
    if (!s) return NULL;
    return stringstore_get_donate(stringstore, s);
}

static const char *_const_strjoin3(stringstore_t *stringstore,
    const char *s1, const char *s2, const char *s3
) {
    char *s = _strjoin3(s1, s2, s3);
    if (!s) return NULL;
    return stringstore_get_donate(stringstore, s);
}

static const char *_const_strjoin4(stringstore_t *stringstore,
    const char *s1, const char *s2, const char *s3, const char *s4
) {
    char *s = _strjoin4(s1, s2, s3, s4);
    if (!s) return NULL;
    return stringstore_get_donate(stringstore, s);
}

#endif