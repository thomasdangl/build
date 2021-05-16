
#ifndef ASM_LEXER_H
#define ASM_LEXER_H

#include <stdlib.h>

typedef struct
{
	char *str;
	char *dup;
	char *p;
	char *next;
} loc_t;

typedef struct
{
	loc_t *loc;
	size_t loc_count;
	loc_t *cur;
} lexer_t;

lexer_t* lexer_init(const char *filename);
lexer_t* lexer_duplicate(lexer_t *lex);
char* lexer_advance(lexer_t *lex);
char* lexer_peek(lexer_t *lex);
size_t lexer_loc(lexer_t *lex);

#endif /* ASM_LEXER_H */

