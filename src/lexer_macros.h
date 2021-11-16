#ifndef _LEXER_MACROS_H_
#define _LEXER_MACROS_H_

#define DO(X) err = (X); if(err)return err;
#define GOT(S) lexer_got(lexer, S)
#define GET(S) DO(lexer_get(lexer, S))
#define OPEN lexer_get_open(lexer)
#define CLOSE lexer_get_close(lexer)
#define NEXT DO(lexer_next(lexer))
#define PARSE_SILENT DO(lexer_parse_silent(lexer))
#define DONE lexer_done(lexer)
#define GOT_NAME lexer_got_name(lexer)
#define GOT_STR lexer_got_str(lexer)
#define GOT_INT lexer_got_int(lexer)
#define GET_NAME(P) DO(lexer_get_name(lexer, (&P)))
#define GET_CONST_NAME(P, STORE) DO(lexer_get_const_name(lexer, (STORE), (&P)))
#define GET_STR(P) DO(lexer_get_str(lexer, (&P)))
#define GET_INT(P) DO(lexer_get_int(lexer, (&P)))
#define UNEXPECTED(S) lexer_unexpected(lexer, S)

#endif