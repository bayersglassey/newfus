#ifndef _LEXER_MACROS_H_
#define _LEXER_MACROS_H_

#define DO(X) {err = (X); if (err) return err;}
#define LOAD(TEXT, FILENAME) DO(lexer_load(lexer, TEXT, FILENAME))
#define GOT(S) lexer_got(lexer, S)
#define GET(S) DO(lexer_get(lexer, S))
#define NEXT DO(lexer_next(lexer))
#define PARSE_SILENT DO(lexer_parse_silent(lexer))
#define DONE lexer_done(lexer)
#define GOT_NAME lexer_got_name(lexer)
#define GOT_OP lexer_got_op(lexer)
#define GOT_STR lexer_got_str(lexer)
#define GOT_INT lexer_got_int(lexer)
#define GOT_OPEN lexer_got_open(lexer)
#define GOT_CLOSE lexer_got_close(lexer)
#define GET_STRING(P) DO(lexer_get_string(lexer, (&P)))
#define GET_CONST_STRING(P, STORE) DO(lexer_get_const_string(lexer, (STORE), (&P)))
#define GET_NAME(P) DO(lexer_get_name(lexer, (&P)))
#define GET_CONST_NAME(P, STORE) DO(lexer_get_const_name(lexer, (STORE), (&P)))
#define GET_OP(P) DO(lexer_get_op(lexer, (&P)))
#define GET_CONST_OP(P, STORE) DO(lexer_get_const_op(lexer, (STORE), (&P)))
#define GET_STR(P) DO(lexer_get_str(lexer, (&P)))
#define GET_CONST_STR(P, STORE) DO(lexer_get_const_str(lexer, (STORE), (&P)))
#define GET_INT(P) DO(lexer_get_int(lexer, (&P)))
#define GET_OPEN DO(lexer_get_open(lexer))
#define GET_CLOSE DO(lexer_get_close(lexer))
#define UNEXPECTED(S) lexer_unexpected(lexer, S)

#endif