#ifndef _WRITER_H_
#define _WRITER_H_


typedef struct writer writer_t;

struct writer {
    FILE *file;

    /* depth of indentation, always >= 0 */
    int depth;

    /* string to be written depth times at start of each line, defaults
    to 4 spaces ("    ") */
    const char *indent_string;

    /* whether to write everything on one line, e.g. (1 (2 3) 4), or using
    indented multiline style with colons instead of parentheses */
    bool oneline;

    /* add_spaces specifies a number of spaces ' ' to write at the beginning
    of each line */
    int add_spaces;

    /* if next token written needs a space in front of it
    (e.g. if writing "(1 2 3)", the "1" doesn't need a space in front if it,
    but "2" and "3" do) */
    bool needs_space;

    /* if next token written needs a newline in front of it */
    bool needs_newline;
};


void writer_cleanup(writer_t *writer);
void writer_init(writer_t *writer, FILE *file);
void writer_reset(writer_t *writer);
int writer_write_name(writer_t *writer, const char *name);
int writer_write_op(writer_t *writer, const char *op);
int writer_write_str(writer_t *writer, const char *s);
int writer_write_blockstr(writer_t *writer, const char *s);
int writer_write_int(writer_t *writer, int i);
int writer_write_open(writer_t *writer);
int writer_write_close(writer_t *writer);



#endif