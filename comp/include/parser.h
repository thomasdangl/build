
#ifndef COMP_PARSER_H
#define COMP_PARSER_H

#include "ast.h"

typedef enum
{
	ident, semic, equals, plus, minus, litint, oparen, cparen, eof
} token_t;

typedef struct
{
	token_t t;
	char *id;
	long long val;
} etoken_t;

typedef struct
{
	char *cur;
	etoken_t tok, lah;
	etoken_t acc, acc2;
	node_t *ast, *scope;
} parser_t;

parser_t* parser_init(const char *filename);
node_t* parser_full_pass(parser_t *par);
void parser_tokenize(parser_t *par);

void parser_program(parser_t *par);
void parser_expression(parser_t *par, node_t *node);
void parser_scope(parser_t *par, node_t *node);

#endif /* COMP_PARSER_H */

