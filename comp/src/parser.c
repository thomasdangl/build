
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

parser_t* parser_init(const char *filename)
{
	parser_t *par =  calloc(1, sizeof(parser_t));
	FILE *fp = fopen(filename, "r");

	if (!fp)
	{
		printf("Failed to open input file `%s`.\n", filename);
		exit(1);
	}

	/* determine eof offset. */
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* read in the file. */
	par->cur = calloc(size + 1, sizeof(char));
	fread(par->cur, 1, size, fp);
	fclose(fp);	

	return par;
}

static char accept(parser_t *par, token_t tok)
{
	if (par->tok.t == tok)
	{
		par->acc = par->tok;
		par->acc2 = (etoken_t) { };
		parser_tokenize(par);
		return 1;
	}
	return 0;
}

static char accept2(parser_t *par, token_t tok, token_t lah)
{
	if (par->tok.t == tok && par->lah.t == lah)
	{
		par->acc = par->tok;
		par->acc2 = par->lah;
		parser_tokenize(par);
		parser_tokenize(par);
		return 1;
	}

	return 0;
}

node_t* parser_full_pass(parser_t *par)
{
	par->ast = ast_init_node(nop, 0);

	/* tokenize until we have arrived at the first token. */
	parser_tokenize(par);
	parser_tokenize(par);

	/* parse program scope. */
	parser_program(par);
	return par->ast;
}

void parser_tokenize(parser_t *par)
{
	par->tok = par->lah;
	memset(&par->lah, 0, sizeof(etoken_t));

	if (!par->cur)
		return;

start:
	par->cur++;

	switch (*par->cur)
	{
	case '\n': case '\t': case '\r': case ' ': goto start;
	case '\0': par->lah.t = eof; par->cur = 0; return;
	case '(':  par->lah.t = oparen; return;
	case ')':  par->lah.t = cparen; return;
	case ';':  par->lah.t = semic; return;
	}

	char *start = par->cur, i = 0, alpha = isalpha(*par->cur) || *par->cur == '_';
	for (; isalnum(*par->cur) || *par->cur == '_'; par->cur++)
		i++;
	
	for (; i > 0;)
	{
		par->cur--;
		par->lah.id = calloc(i + 1, 1);
		strncpy(par->lah.id, start, i);

		if (alpha)
			par->lah.t = ident;
		else
		{
			if (par->lah.id[0] == '0' && (par->lah.id[1] == 'x' || par->lah.id[1] == 'X'))
				par->lah.val = strtoull(par->lah.id + 2, 0, 16);
			else if (par->lah.id[0] == '0' && (par->lah.id[1] == 'b' || par->lah.id[1] == 'B'))
				par->lah.val = strtoull(par->lah.id + 2, 0, 2);
			else
				par->lah.val = strtoull(par->lah.id, 0, 10);
		
			if (par->lah.val == 0 && (i != 1 || par->lah.id[0] != '0'))
				break;
			par->lah.t = litint;
			par->lah.id = 0;
		}

		return;
	}
	
	printf("Failed to tokenize `%c`.\n", *par->cur);
	exit(1);
}

void parser_program(parser_t *par)
{
	if (accept2(par, ident, oparen))
	{
		const char *fn = par->acc.id;

		/* TODO: where should this plug into? */
		node_t *node = ast_init_node(call, par->ast);

		/* TODO: accept any expression here */
		while (accept(par, litint))
		{
			ast_init_node(constant, node)->constant.val = par->acc.val;
		}

		/* function invokation */
		if (accept2(par, cparen, semic))
		{
			node->call.name = fn;
			return;	
		}
	}
	
	printf("Failed to parse program.\n");
	exit(1);
}

void parser_scope(parser_t *par)
{

	printf("Failed to parse scope.\n");
	exit(1);
}

