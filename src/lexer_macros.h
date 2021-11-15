#ifndef _LEXER_MACROS_H_
#define _LEXER_MACROS_H_

#define DO(X) err = (X); if(err)return err;
#define GOT(S) fus_lexer_got(lexer, S)
#define GET(S) DO(fus_lexer_get(lexer, S))
#define OPEN GET("(")
#define CLOSE GET(")")
#define NEXT DO(fus_lexer_next(lexer))
#define PARSE_SILENT DO(fus_lexer_parse_silent(lexer))
#define DONE fus_lexer_done(lexer)
#define GOT_NAME fus_lexer_got_name(lexer)
#define GOT_STR fus_lexer_got_str(lexer)
#define GOT_INT fus_lexer_got_int(lexer)
#define GET_NAME(P) DO(fus_lexer_get_name(lexer, (&P)))
#define GET_STR(P) DO(fus_lexer_get_str(lexer, (&P)))
#define GET_INT(P) DO(fus_lexer_get_int_fancy(lexer, (&P)))
#define UNEXPECTED(S) fus_lexer_unexpected(lexer, S)

#endif