

#include "lexer.h"


static size_t _strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len] != '\0') len++;
    return len;
}

static char *_strndup(const char *s1, size_t len) {
    size_t s_len = _strnlen(s1, len);
    char *s2 = malloc(s_len + 1);
    if (s2 == NULL) return NULL;
    strncpy(s2, s1, len);
    s2[s_len] = '\0';
    return s2;
}



const int INITIAL_INDENTS_SIZE = 32;

static int lexer_get_indent(lexer_t *lexer);

void lexer_cleanup(lexer_t *lexer) {
    free(lexer->indents);
}

void lexer_init(lexer_t *lexer) {
    memset(lexer, 0, sizeof(*lexer));
    lexer->token_type = LEXER_TOKEN_DONE;
}

void lexer_dump(lexer_t *lexer, FILE *f) {
    fprintf(f, "lexer: %p\n", lexer);
    if (lexer == NULL) return;
    /* fprintf(f, "  text = ...\n"); */
    fprintf(f, "  pos = %i\n", lexer->pos);
    fprintf(f, "  row = %i\n", lexer->row);
    fprintf(f, "  col = %i\n", lexer->col);
    fprintf(f, "  indent = %i\n", lexer->indent);
    fprintf(f, "  indents_size = %i\n", lexer->indents_size);
    fprintf(f, "  n_indents = %i\n", lexer->n_indents);
    fprintf(f, "  indents:\n");
    for (int i = 0; i < lexer->n_indents; i++) {
        fprintf(f, "    %i\n", lexer->indents[i]);
    }
    fprintf(f, "  returning_indents = %i\n", lexer->returning_indents);
}

void lexer_info(lexer_t *lexer, FILE *f) {
    fprintf(f, "%s: row %i: col %i: ",
        lexer->filename,
        lexer->row + 1,
        lexer->col - lexer->token_len + 1);
}

void lexer_err_info(lexer_t *lexer) {
    fprintf(stderr, "Lexer error: ");
    lexer_info(lexer, stderr);
}

int lexer_load(lexer_t *lexer, const char *text,
    const char *filename
) {
    int err;

    if (lexer_loaded(lexer)) lexer_unload(lexer);

    if (lexer->indents_size == 0) {
        int indents_size = INITIAL_INDENTS_SIZE;
        int *indents = calloc(indents_size, sizeof(*indents));
        if (indents == NULL) return 1;

        lexer->indents_size = indents_size;
        lexer->indents = indents;
    }

    lexer->filename = filename;
    lexer->text = text;
    lexer->text_len = strlen(text);
    lexer->token_type = LEXER_TOKEN_DONE;

    err = lexer_get_indent(lexer);
    if (err) return err;
    err = lexer_next(lexer);
    if (err) return err;
    return 0;
}

void lexer_unload(lexer_t *lexer) {
    lexer->filename = NULL;
    lexer->text_len = 0;
    lexer->text = NULL;
    lexer->token_len = 0;
    lexer->token = NULL;
    lexer->pos = 0;
    lexer->row = 0;
    lexer->col = 0;
    lexer->indent = 0;
    lexer->n_indents = 0;
    lexer->returning_indents = 0;
}

bool lexer_loaded(lexer_t *lexer) {
    return lexer->text != NULL;
}

static void lexer_start_token(lexer_t *lexer) {
    lexer->token = lexer->text + lexer->pos;
    lexer->token_len = 0;
}

static void lexer_end_token(lexer_t *lexer) {
    int token_startpos = lexer->token - lexer->text;
    lexer->token_len = lexer->pos - token_startpos;
}

static void lexer_set_token(
    lexer_t *lexer,
    const char *token
) {
    lexer->token = token;
    lexer->token_len = strlen(token);
}

static int lexer_push_indent(
    lexer_t *lexer,
    int indent
) {
    if (lexer->n_indents >= lexer->indents_size) {
        int indents_size = lexer->indents_size;
        int new_indents_size = indents_size * 2;
        int *new_indents = realloc(lexer->indents,
            new_indents_size * sizeof(*new_indents));
        if (new_indents == NULL) return 1;
        memset(new_indents + indents_size, 0,
            (new_indents_size - indents_size) * sizeof(*new_indents));

        lexer->indents = new_indents;
        lexer->indents_size = new_indents_size;
    }

    lexer->n_indents++;
    lexer->indents[lexer->n_indents-1] = indent;
    return 0;
}

static int lexer_pop_indent(lexer_t *lexer) {
    if (lexer->n_indents == 0) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Tried to pop an indent, but indents stack is empty\n");
        return 2;
    }
    lexer->n_indents--;
    return 0;
}


static char lexer_peek(lexer_t *lexer) {
    return lexer->text[lexer->pos + 1];
}

static char lexer_eat(lexer_t *lexer) {
    char c = lexer->text[lexer->pos];
    lexer->pos++;
    if (c == '\n') {
        lexer->row++;
        lexer->col = 0;
    } else{
        lexer->col++;
    }
    return c;
}

static int lexer_get_indent(lexer_t *lexer) {
    int indent = 0;
    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (c == ' ') {
            indent++;
            lexer_eat(lexer);
        } else if (c == '\n') {
            /* blank lines don't count towards indentation --
            just reset the indentation and restart on next line */
            indent = 0;
            lexer_eat(lexer);
        } else if (c != '\0' && isspace(c)) {
            lexer_err_info(lexer);
            fprintf(stderr,
                "Indented with whitespace other than ' ' "
                "(#32): #%i\n", (int)c);
            return 2;
        } else{
            break;
        }
    }
    lexer->indent = indent;
    return 0;
}

static void lexer_eat_whitespace(lexer_t *lexer) {
    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (c == '\0' || isgraph(c)) break;
        lexer_eat(lexer);
    }
}

static void lexer_eat_comment(lexer_t *lexer) {
    /* eat leading '#' */
    lexer_eat(lexer);

    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (c == '\n') break;
        lexer_eat(lexer);
    }
}

static void lexer_parse_name(lexer_t *lexer) {
    lexer_start_token(lexer);
    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (c != '_' && !isalnum(c)) break;
        lexer_eat(lexer);
    }
    lexer_end_token(lexer);
}

static void lexer_parse_int(lexer_t *lexer) {
    lexer_start_token(lexer);

    /* eat leading '-' if present */
    if (lexer->text[lexer->pos] == '-') lexer_eat(lexer);

    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (!isdigit(c)) break;
        lexer_eat(lexer);
    }
    lexer_end_token(lexer);
}

static void lexer_parse_op(lexer_t *lexer) {
    lexer_start_token(lexer);
    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (c == '(' || c == ')' || c == ':'
            || !isgraph(c) || isalnum(c)) break;
        lexer_eat(lexer);
    }
    lexer_end_token(lexer);
}

static int lexer_parse_str(lexer_t *lexer) {
    lexer_start_token(lexer);

    /* Include leading '"' */
    lexer_eat(lexer);

    while (1) {
        char c = lexer->text[lexer->pos];
        if (c == '\0') {
            goto err_eof;
        } else if (c == '\n') {
            goto err_eol;
        } else if (c == '"') {
            lexer_eat(lexer);
            break;
        } else if (c == '\\') {
            lexer_eat(lexer);
            char c = lexer->text[lexer->pos];
            if (c == '\0') {
                goto err_eof;
            } else if (c == '\n') {
                goto err_eol;
            }
        }
        lexer_eat(lexer);
    }
    lexer_end_token(lexer);
    return 0;
err_eol:
    lexer_err_info(lexer);
    fprintf(stderr,
        "Reached newline while parsing str\n");
    return 2;
err_eof:
    lexer_err_info(lexer);
    fprintf(stderr,
        "Reached end of text while parsing str\n");
    return 2;
}

static int lexer_parse_blockstr(lexer_t *lexer) {
    lexer_start_token(lexer);

    /* Include leading ";;" */
    lexer_eat(lexer);
    lexer_eat(lexer);

    while (lexer->pos < lexer->text_len) {
        char c = lexer->text[lexer->pos];
        if (c == '\0') {
            break;
        } else if (c == '\n') {
            break;
        }
        lexer_eat(lexer);
    }
    lexer_end_token(lexer);
    return 0;
}

int lexer_next(lexer_t *lexer) {
    int err;
    while (1) {
        /* return "(" or ")" token based on indents? */
        if (lexer->returning_indents != 0) break;

        char c = lexer->text[lexer->pos];
        if (c == '\0' || c == '\n') {
            if (c == '\n') lexer_eat(lexer);

            err = lexer_get_indent(lexer);
            if (err) return err;
            int new_indent = lexer->indent;
            while (lexer->n_indents > 0) {
                int indent = lexer->indents[lexer->n_indents-1];
                if (new_indent <= indent) {
                    err = lexer_pop_indent(lexer);
                    if (err) return err;
                    lexer->returning_indents--;
                } else{
                    break;
                }
            }

            if (c == '\0') {
                /* Reached end of file; report with NULL token */
                lexer->token = NULL;
                lexer->token_len = 0;
                lexer->token_type = LEXER_TOKEN_DONE;
                break;
            }
        } else if (isspace(c)) {
            lexer_eat_whitespace(lexer);
        } else if (c == ':') {
            lexer_eat(lexer);
            lexer->returning_indents++;
            lexer_push_indent(lexer, lexer->indent);
            break;
        } else if (c == '(' || c == ')') {
            lexer_start_token(lexer);
            lexer_eat(lexer);
            lexer_end_token(lexer);
            lexer->token_type = c == '('? LEXER_TOKEN_OPEN: LEXER_TOKEN_CLOSE;
            break;
        } else if (c == '_' || isalpha(c)) {
            lexer_parse_name(lexer);
            lexer->token_type = LEXER_TOKEN_NAME;
            break;
        } else if (isdigit(c) || (
            c == '-' && isdigit(lexer_peek(lexer))
        )) {
            lexer_parse_int(lexer);
            lexer->token_type = LEXER_TOKEN_INT;
            break;
        } else if (c == '#') {
            lexer_eat_comment(lexer);
        } else if (c == '"') {
            err = lexer_parse_str(lexer);
            if (err) return err;
            lexer->token_type = LEXER_TOKEN_STR;
            break;
        } else if (c == ';' && lexer_peek(lexer) == ';') {
            err = lexer_parse_blockstr(lexer);
            if (err) return err;
            lexer->token_type = LEXER_TOKEN_BLOCKSTR;
            break;
        } else{
            lexer_parse_op(lexer);
            lexer->token_type = LEXER_TOKEN_OP;
            break;
        }
    }

    if (lexer->returning_indents > 0) {
        lexer->token_type = LEXER_TOKEN_OPEN;
        lexer_set_token(lexer, "(");
        lexer->returning_indents--;
    }
    if (lexer->returning_indents < 0) {
        lexer->token_type = LEXER_TOKEN_CLOSE;
        lexer_set_token(lexer, ")");
        lexer->returning_indents++;
    }

    return 0;
}

bool lexer_done(lexer_t *lexer) {
    return lexer->token == NULL;
}

bool lexer_got(lexer_t *lexer, const char *text) {
    if (lexer->token == NULL || text == NULL) {
        return lexer->token == text;
    }
    return
        lexer->token_len == strlen(text) &&
        strncmp(lexer->token, text, lexer->token_len) == 0
    ;
}

bool lexer_got_name(lexer_t *lexer) {
    return lexer->token_type == LEXER_TOKEN_NAME;
}

bool lexer_got_op(lexer_t *lexer) {
    return lexer->token_type == LEXER_TOKEN_OP;
}

bool lexer_got_str(lexer_t *lexer) {
    return
        lexer->token_type == LEXER_TOKEN_STR ||
        lexer->token_type == LEXER_TOKEN_BLOCKSTR;
}

bool lexer_got_int(lexer_t *lexer) {
    return lexer->token_type == LEXER_TOKEN_INT;
}

bool lexer_got_open(lexer_t *lexer) {
    return lexer->token_type == LEXER_TOKEN_OPEN;
}

bool lexer_got_close(lexer_t *lexer) {
    return lexer->token_type == LEXER_TOKEN_CLOSE;
}

void lexer_show(lexer_t *lexer, FILE *f) {
    if (lexer->token == NULL) {
        fprintf(f, "end of input");
    } else{
        fprintf(f, "\"%.*s\"", lexer->token_len, lexer->token);
    }
}

int lexer_get(lexer_t *lexer, const char *text) {
    if (!lexer_got(lexer, text)) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected \"%s\", but got: ", text);
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return lexer_next(lexer);
}

static int _lexer_get_string(lexer_t *lexer, char **string) {
    /* Caller guarantees lexer->token_type is LEXER_TOKEN_{NAME,OP} */
    *string = _strndup(lexer->token, lexer->token_len);
    if (*string == NULL) return 1;
    return lexer_next(lexer);
}

int lexer_get_string(lexer_t *lexer, char **string) {
    if (
        lexer->token_type == LEXER_TOKEN_STR ||
        lexer->token_type == LEXER_TOKEN_BLOCKSTR
    ) {
        return lexer_get_str(lexer, string);
    } else if (
        lexer->token_type == LEXER_TOKEN_NAME ||
        lexer->token_type == LEXER_TOKEN_OP
    ) {
        return _lexer_get_string(lexer, string);
    } else {
        lexer_err_info(lexer);
        fprintf(stderr, "Expected name or op or str, but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
}

int lexer_get_const_string(lexer_t *lexer, stringstore_t *stringstore,
    const char **string
) {
    int err;

    char *_string;
    err = lexer_get_string(lexer, &_string);
    if (err) return err;

    const char *const_string = stringstore_get_donate(stringstore, _string);
    if (!const_string) return 1;

    *string = const_string;
    return 0;
}

int lexer_get_name(lexer_t *lexer, char **name) {
    if (!lexer_got_name(lexer)) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected name, but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return _lexer_get_string(lexer, name);
}

int lexer_get_const_name(lexer_t *lexer, stringstore_t *stringstore,
    const char **name
) {
    int err;

    char *_name;
    err = lexer_get_name(lexer, &_name);
    if (err) return err;

    const char *const_name = stringstore_get_donate(stringstore, _name);
    if (!const_name) return 1;

    *name = const_name;
    return 0;
}

int lexer_get_op(lexer_t *lexer, char **op) {
    if (!lexer_got_op(lexer)) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected op, but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return _lexer_get_string(lexer, op);
}

int lexer_get_const_op(lexer_t *lexer, stringstore_t *stringstore,
    const char **op
) {
    int err;

    char *_op;
    err = lexer_get_op(lexer, &_op);
    if (err) return err;

    const char *const_op = stringstore_get_donate(stringstore, _op);
    if (!const_op) return 1;

    *op = const_op;
    return 0;
}

int lexer_get_str(lexer_t *lexer, char **s) {
    if (lexer->token_type == LEXER_TOKEN_STR) {
        const char *token = lexer->token;
        int token_len = lexer->token_len;

        /* Length of s is length of token without the surrounding
        '"' characters */
        int s_len = token_len - 2;

        char *ss0 = malloc(s_len + 1);
        if (ss0 == NULL) return 1;
        char *ss = ss0;

        for (int i = 1; i < token_len - 1; i++) {
            char c = token[i];
            if (c == '\\') {
                i++;
                c = token[i];
            }
            *ss = c;
            ss++;
        }

        *ss = '\0';
        *s = ss0;
    } else if (lexer->token_type == LEXER_TOKEN_BLOCKSTR) {
        const char *token = lexer->token;
        int token_len = lexer->token_len;

        /* Length of s is length of token without the leading ";;" */
        int s_len = token_len - 2;

        char *ss = _strndup(token+2, s_len);
        if (ss == NULL) return 1;

        *s = ss;
    } else{
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected str, but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return lexer_next(lexer);
}

int lexer_get_const_str(lexer_t *lexer, stringstore_t *stringstore,
    const char **s
) {
    int err;

    char *_s;
    err = lexer_get_str(lexer, &_s);
    if (err) return err;

    const char *cs = stringstore_get_donate(stringstore, _s);
    if (!cs) return 1;

    *s = cs;
    return 0;
}

int lexer_get_int(lexer_t *lexer, int *i) {
    if (!lexer_got_int(lexer)) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected int, but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    *i = atoi(lexer->token);
    return lexer_next(lexer);
}

int lexer_get_open(lexer_t *lexer) {
    if (!lexer_got_open(lexer)) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected '(', but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return lexer_next(lexer);
}

int lexer_get_close(lexer_t *lexer) {
    if (!lexer_got_close(lexer)) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected ')', but got: ");
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return lexer_next(lexer);
}

int lexer_unexpected(lexer_t *lexer, const char *expected) {
    lexer_err_info(lexer);
    if (expected == NULL) fprintf(stderr, "Unexpected: ");
    else fprintf(stderr, "Expected %s, but got: ", expected);
    lexer_show(lexer, stderr);
    fprintf(stderr, "\n");
    return 2;
}

int lexer_parse_silent(lexer_t *lexer) {
    int depth = 1;
    while (1) {
        int err;

        if (lexer_got(lexer, "(")) {
            err = lexer_next(lexer);
            if (err) return err;
            depth++;
        } else if (lexer_got(lexer, ")")) {
            depth--;
            if (depth == 0) break;
            err = lexer_next(lexer);
            if (err) return err;
        } else if (lexer_done(lexer)) {
            return lexer_unexpected(lexer, NULL);
        } else{
            /* eat atoms silently */
            err = lexer_next(lexer);
            if (err) return err;
        }
    }
    return 0;
}
