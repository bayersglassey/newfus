
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "lexer.h"
#include "str_utils.h"
#include "stringstore.h"
#include "tokentree.h"


struct tokentree_frame {
    tokentree_t *tokentree; /* TOKENTREE_TAG_ARR */
    int i; /* Index within tokentree */
};


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

static int lexer_token_type_from_tokentree_tag(int tag) {
    /* Converts enum tokentree_tag to enum lexer_token_type */
    switch (tag) {
        case TOKENTREE_TAG_INT: return LEXER_TOKEN_INT;
        case TOKENTREE_TAG_NAME: return LEXER_TOKEN_NAME;
        case TOKENTREE_TAG_OP: return LEXER_TOKEN_OP;
        case TOKENTREE_TAG_STR: return LEXER_TOKEN_STR;
        case TOKENTREE_TAG_ARR: return LEXER_TOKEN_OPEN;
        default: return LEXER_TOKEN_TYPES; /* Invalid value */
    }
}



const int INITIAL_INDENTS_SIZE = 32;
const int INITIAL_TOKENTREE_FRAMES_SIZE = 32;

static int lexer_get_indent(lexer_t *lexer);

void lexer_cleanup(lexer_t *lexer) {
    free(lexer->indents);
    free(lexer->tokentree_frames);
}

void lexer_init(lexer_t *lexer, stringstore_t *store) {
    memset(lexer, 0, sizeof(*lexer));
    lexer->token_type = LEXER_TOKEN_DONE;
    lexer->store = store;
}

void lexer_dump(lexer_t *lexer, FILE *f) {
    fprintf(f, "lexer: %p\n", lexer);
    if (lexer == NULL) return;
    /* fprintf(f, "  text = ...\n"); */
    fprintf(f, "  filename = %s\n", lexer->filename? lexer->filename: "(none)");
    fprintf(f, "  pos = %i\n", lexer->pos);
    fprintf(f, "  row = %i\n", lexer->row);
    fprintf(f, "  col = %i\n", lexer->col);
    fprintf(f, "  returning_indents = %i\n", lexer->returning_indents);
    fprintf(f, "  indent = %i\n", lexer->indent);
    fprintf(f, "  indents_size = %i\n", lexer->indents_size);
    fprintf(f, "  indents_len = %i\n", lexer->indents_len);
    fprintf(f, "  indents:\n");
    for (int i = 0; i < lexer->indents_len; i++) {
        fprintf(f, "    %i\n", lexer->indents[i]);
    }

    fprintf(f, "  loaded_tokentree = ");
    if (lexer->loaded_tokentree) tokentree_write(lexer->loaded_tokentree, f, -1);
    else fprintf(f, "(none)");
    fprintf(f, "\n");

    if (lexer->loaded_tokentree) {
        fprintf(f, "  tokentree = ");
        if (lexer->tokentree) tokentree_write(lexer->tokentree, f, -1);
        else fprintf(f, "(none)");
        fprintf(f, "\n");
        fprintf(f, "  tokentree_frames_size = %i\n", lexer->tokentree_frames_size);
        fprintf(f, "  tokentree_frames_len = %i\n", lexer->tokentree_frames_len);
        fprintf(f, "  tokentree_frames:\n");
        for (int i = 0; i < lexer->tokentree_frames_len; i++) {
            tokentree_frame_t *frame = &lexer->tokentree_frames[i];
            fprintf(f, "    [%i] ", frame->i);
            tokentree_write(frame->tokentree, f, -1);
            fprintf(f, "\n");
        }
    }
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

int lexer_load_tokentree(lexer_t *lexer, tokentree_t *tokentree,
    const char *filename
) {
    if (lexer_loaded(lexer)) lexer_unload(lexer);

    if (lexer->tokentree_frames_size == 0) {
        int tokentree_frames_size = INITIAL_TOKENTREE_FRAMES_SIZE;
        tokentree_frame_t *tokentree_frames = calloc(
            tokentree_frames_size, sizeof(*tokentree_frames));
        if (tokentree_frames == NULL) return 1;

        lexer->tokentree_frames_size = tokentree_frames_size;
        lexer->tokentree_frames = tokentree_frames;
    }

    lexer->filename = filename;
    lexer->token_type = lexer_token_type_from_tokentree_tag(
        tokentree->tag);

    lexer->loaded_tokentree = tokentree;
    lexer->tokentree = tokentree;
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
    lexer->returning_indents = 0;
    lexer->indent = 0;
    lexer->indents_len = 0;
    lexer->loaded_tokentree = NULL;
    lexer->tokentree = NULL;
    lexer->tokentree_frames_len = 0;
}

bool lexer_loaded(lexer_t *lexer) {
    return lexer->loaded_tokentree || lexer->text;
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
    if (lexer->indents_len >= lexer->indents_size) {
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

    lexer->indents_len++;
    lexer->indents[lexer->indents_len-1] = indent;
    return 0;
}

static int lexer_pop_indent(lexer_t *lexer) {
    if (lexer->indents_len == 0) {
        lexer_err_info(lexer);
        fprintf(stderr,
            "Tried to pop an indent, but indents stack is empty\n");
        return 2;
    }
    lexer->indents_len--;
    return 0;
}

static tokentree_frame_t *lexer_get_tokentree_frame(lexer_t *lexer) {
    if (lexer->tokentree_frames_len == 0) return NULL;
    return &lexer->tokentree_frames[lexer->tokentree_frames_len - 1];
}

static int lexer_push_tokentree_frame(
    lexer_t *lexer,
    tokentree_t *tokentree
) {
    assert(tokentree->tag == TOKENTREE_TAG_ARR);

    if (lexer->tokentree_frames_len >= lexer->tokentree_frames_size) {
        int tokentree_frames_size = lexer->tokentree_frames_size;
        int new_tokentree_frames_size = tokentree_frames_size * 2;
        tokentree_frame_t *new_tokentree_frames = realloc(lexer->tokentree_frames,
            new_tokentree_frames_size * sizeof(*new_tokentree_frames));
        if (new_tokentree_frames == NULL) return 1;
        memset(new_tokentree_frames + tokentree_frames_size, 0,
            (new_tokentree_frames_size - tokentree_frames_size)
                * sizeof(*new_tokentree_frames));

        lexer->tokentree_frames = new_tokentree_frames;
        lexer->tokentree_frames_size = new_tokentree_frames_size;
    }

    lexer->tokentree_frames_len++;
    tokentree_frame_t *frame = &lexer->tokentree_frames[
        lexer->tokentree_frames_len - 1];
    frame->tokentree = tokentree;
    frame->i = 0;
    return 0;
}

static void lexer_pop_tokentree_frame(lexer_t *lexer) {
    /* Caller guarantees that lexer->tokentree_frames_len > 0 */
    lexer->tokentree_frames_len--;
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
    } else {
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
        } else {
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

static int lexer_next_tokentree(lexer_t *lexer) {
    /* Set lexer->tokentree to its "next" value, and set lexer->token_type
    accordingly. */
    int err;

    if (lexer->tokentree == NULL) {
        /* We already visited everything in lexer->loaded_tokentree, so now
        we keep returning LEXER_TOKEN_DONE forever. */
        lexer->token_type = LEXER_TOKEN_DONE;
        return 0;
    }

    if (lexer->token_type == LEXER_TOKEN_OPEN) {
        /* Previously, we returned LEXER_TOKEN_OPEN.
        Now we "open" lexer->tokentree, which is guaranteed to be an
        array (TOKENTREE_TAG_ARR), and return its first element (if any) or
        LEXER_TYPE_CLOSE (if it has no elements). */

        if (lexer->tokentree->u.array_f.len == 0) {
            lexer->token_type = LEXER_TOKEN_CLOSE;
            return 0;
        }

        tokentree_t *new_tokentree = &lexer->tokentree->u.array_f.elems[0];
        err = lexer_push_tokentree_frame(lexer, lexer->tokentree);
        if (err) return err;
        lexer->tokentree = new_tokentree;
        lexer->token_type = lexer_token_type_from_tokentree_tag(
            lexer->tokentree->tag);
        return 0;
    }

    /* Try to get the current frame, so we can continue iterating
    over its tokentree */
    tokentree_frame_t *frame = lexer_get_tokentree_frame(lexer);
    if (!frame) {
        /* No frame: we're done */
        lexer->token_type = LEXER_TOKEN_DONE;
        return 0;
    }

    if (frame->i >= frame->tokentree->u.array_f.len - 1) {
        /* Frame's tokentree is already fully iterated over: pop it */
        lexer_pop_tokentree_frame(lexer);
        lexer->tokentree = frame->tokentree;
        lexer->token_type = LEXER_TOKEN_CLOSE;
        return 0;
    }

    /* Iterate */
    frame->i++;
    lexer->tokentree = &frame->tokentree->u.array_f.elems[frame->i];
    lexer->token_type = lexer_token_type_from_tokentree_tag(
        lexer->tokentree->tag);
    return 0;
}

int lexer_next(lexer_t *lexer) {
    int err;

    if (lexer->loaded_tokentree) return lexer_next_tokentree(lexer);

    while (1) {
        /* return "(" or ")" token based on indents? */
        if (lexer->returning_indents != 0) break;

        char c = lexer->text[lexer->pos];
        if (c == '\0' || c == '\n') {
            if (c == '\n') lexer_eat(lexer);

            err = lexer_get_indent(lexer);
            if (err) return err;
            int new_indent = lexer->indent;
            while (lexer->indents_len > 0) {
                int indent = lexer->indents[lexer->indents_len-1];
                if (new_indent <= indent) {
                    err = lexer_pop_indent(lexer);
                    if (err) return err;
                    lexer->returning_indents--;
                } else {
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
        } else {
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
    return lexer->token_type == LEXER_TOKEN_DONE;
}

bool lexer_got(lexer_t *lexer, const char *text) {
    if (text == NULL) return lexer_done(lexer);

    /* NOTE: lexer_got shouldn't be used with text representing a
    STR or BLOCKSTR.
    It's really only meant for use with names and ops, although we
    allow using it with DONE, OPEN, CLOSE, and INT since those are
    easy enough to check for. */

    if (lexer->loaded_tokentree) {
        if (text[0] == '(') {
            return lexer->token_type == LEXER_TOKEN_OPEN;
        } else if (text[0] == ')') {
            return lexer->token_type == LEXER_TOKEN_CLOSE;
        } else if (isdigit(text[0]) || (
            text[0] == '-' && isdigit(text[1])
        )) {
            if (lexer->token_type != LEXER_TOKEN_INT) return false;

            /* Kind of ganky, but I believe it works (TODO: make sure...) */
            int i = atoi(text);
            return lexer->tokentree->u.int_f == i;
        } else {
            if (!lexer->tokentree) return false;
            if (!tokentree_tag_is_string(lexer->tokentree->tag)) return false;
            return !strcmp(lexer->tokentree->u.string_f, text);
        }
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

bool lexer_got_string(lexer_t *lexer) {
    return
        lexer_got_name(lexer) ||
        lexer_got_op(lexer) ||
        lexer_got_str(lexer);
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
    if (lexer->tokentree) {
        tokentree_write(lexer->tokentree, f, -1);
    } else if (lexer->token) {
        fprintf(f, "\"%.*s\"", lexer->token_len, lexer->token);
    } else {
        fprintf(f, "end of input");
    }
}

int lexer_get(lexer_t *lexer, const char *text) {
    if (!lexer_got(lexer, text)) {
        /* The following is basically lexer_unexpected, but with quotes */
        lexer_err_info(lexer);
        fprintf(stderr,
            "Expected \"%s\", but got: ", text);
        lexer_show(lexer, stderr);
        fprintf(stderr, "\n");
        return 2;
    }
    return lexer_next(lexer);
}

static int _lexer_get_name_or_op(lexer_t *lexer, char **string) {
    /* NOTE: caller guarantees lexer->token_type is LEXER_TOKEN_{NAME,OP} */
    if (lexer->tokentree) {
        *string = _strdup(lexer->tokentree->u.string_f);
        if (*string == NULL) return 1;
    } else {
        *string = _strndup(lexer->token, lexer->token_len);
        if (*string == NULL) return 1;
    }
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
        return _lexer_get_name_or_op(lexer, string);
    } else {
        return lexer_unexpected(lexer, "name or op or str");
    }
}

static int _lexer_get_const_name_or_op(lexer_t *lexer, const char **string) {
    /* NOTE: caller guarantees lexer->token_type is LEXER_TOKEN_{NAME,OP} */

    if (lexer->tokentree) {
        *string = lexer->tokentree->u.string_f;
        return lexer_next(lexer);
    }

    if (!lexer->store) {
        fprintf(stderr, "%s: Lexer requires stringstore\n", __func__);
        return 2;
    }

    char *_string = _strndup(lexer->token, lexer->token_len);
    if (_string == NULL) return 1;

    const char *const_string = stringstore_get_donate(lexer->store,
        _string);
    if (!const_string) return 1;

    *string = const_string;
    return lexer_next(lexer);
}

int lexer_get_const_string(lexer_t *lexer, const char **string) {
    if (
        lexer->token_type == LEXER_TOKEN_STR ||
        lexer->token_type == LEXER_TOKEN_BLOCKSTR
    ) {
        return lexer_get_const_str(lexer, string);
    } else if (
        lexer->token_type == LEXER_TOKEN_NAME ||
        lexer->token_type == LEXER_TOKEN_OP
    ) {
        return _lexer_get_const_name_or_op(lexer, string);
    } else {
        return lexer_unexpected(lexer, "name or op or str");
    }
}

int lexer_get_name(lexer_t *lexer, char **name) {
    if (!lexer_got_name(lexer)) return lexer_unexpected(lexer, "name");
    return _lexer_get_name_or_op(lexer, name);
}

int lexer_get_const_name(lexer_t *lexer, const char **name) {
    if (!lexer_got_name(lexer)) return lexer_unexpected(lexer, "name");
    return _lexer_get_const_name_or_op(lexer, name);
}

int lexer_get_op(lexer_t *lexer, char **op) {
    if (!lexer_got_op(lexer)) return lexer_unexpected(lexer, "op");
    return _lexer_get_name_or_op(lexer, op);
}

int lexer_get_const_op(lexer_t *lexer, const char **op) {
    if (!lexer_got_op(lexer)) return lexer_unexpected(lexer, "op");
    return _lexer_get_const_name_or_op(lexer, op);
}

int lexer_get_str(lexer_t *lexer, char **s) {
    if (!lexer_got_str(lexer)) return lexer_unexpected(lexer, "str");

    if (lexer->tokentree) {
        *s = _strdup(lexer->tokentree->u.string_f);
        if (*s == NULL) return 1;
        return lexer_next(lexer);
    }

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
    }
    return lexer_next(lexer);
}

int lexer_get_const_str(lexer_t *lexer, const char **s) {
    int err;

    if (!lexer_got_str(lexer)) return lexer_unexpected(lexer, "str");

    if (lexer->tokentree) {
        *s = lexer->tokentree->u.string_f;
        return lexer_next(lexer);
    }

    if (!lexer->store) {
        fprintf(stderr, "%s: Lexer requires stringstore\n", __func__);
        return 2;
    }

    char *_s;
    err = lexer_get_str(lexer, &_s);
    if (err) return err;

    const char *cs = stringstore_get_donate(lexer->store, _s);
    if (!cs) return 1;

    *s = cs;
    return lexer_next(lexer);
}

int lexer_get_int(lexer_t *lexer, int *i) {
    if (!lexer_got_int(lexer)) return lexer_unexpected(lexer, "int");
    *i = lexer->tokentree? lexer->tokentree->u.int_f: atoi(lexer->token);
    return lexer_next(lexer);
}

int lexer_get_open(lexer_t *lexer) {
    if (!lexer_got_open(lexer)) return lexer_unexpected(lexer, "'('");
    return lexer_next(lexer);
}

int lexer_get_close(lexer_t *lexer) {
    if (!lexer_got_close(lexer)) return lexer_unexpected(lexer, "')'");
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

        if (lexer_got_open(lexer)) {
            err = lexer_next(lexer);
            if (err) return err;
            depth++;
        } else if (lexer_got_close(lexer)) {
            depth--;
            if (depth == 0) break;
            err = lexer_next(lexer);
            if (err) return err;
        } else if (lexer_done(lexer)) {
            return lexer_unexpected(lexer, NULL);
        } else {
            /* eat atoms silently */
            err = lexer_next(lexer);
            if (err) return err;
        }
    }
    return 0;
}
