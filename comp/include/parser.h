
#ifndef COMP_PARSER_H
#define COMP_PARSER_H

#include "ast.h"

typedef enum
{
	ident, semic, equals, plus, minus, times, frac, litint, oparen, cparen,
	ocurl, ccurl, quot, ret, eof
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
char parser_lexpression(parser_t *par, node_t *node);
char parser_rexpression(parser_t *par, node_t *node);
char parser_scope(parser_t *par, node_t *node);
char parser_eq(parser_t *par, node_t *node);
char parser_assign(parser_t *par, node_t *node);
char parser_fn(parser_t *par, node_t *node, char *id);
char parser_call(parser_t *par, node_t *node);
char parser_val(parser_t *par, node_t *node);
char parser_arith(parser_t *par, node_t *node);
char parser_return(parser_t *par, node_t *node);

#endif /* COMP_PARSER_H */

