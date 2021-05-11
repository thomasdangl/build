
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
	par->ast = ast_init_node(scope, 0);

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
	case '=':  par->lah.t = equals; return;
	case '+':  par->lah.t = plus; return;
	case '-':  par->lah.t = minus; return;
	case '/': 
		if (*(par->cur + 1) == '*')
		{
			while (*par->cur != '\0' &&
				(*par->cur != '*' || *(par->cur + 1) != '/'))
				par->cur++;
			par->cur++;
		}
		else if (*(par->cur + 1) == '/')
			while (*par->cur != '\0' && *par->cur != '\n')
				par->cur++;
		else
		{
			printf("Failed to parse comment!\n");
			exit(1);
		}
		
		goto start;
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
	if (parser_scope(par, par->ast))
	{
		printf("Failed to parse scope.\n");
		exit(1);
	}
}

char parser_expression(parser_t *par, node_t *node)
{
	if (accept2(par, ident, equals))
	{
		node_t *new = ast_init_node(assign, node);
		
		if (accept(par, equals))
		{
			/* TODO: */
		}

		ast_init_node(variable, new)->variable.sym =
			ast_symbolize(par->scope, par->acc.id, 1);
		parser_expression(par, new);

		if (!accept(par, semic))
		{
			printf("Missing semicolon.\n");
			exit(1);
		}

		return 0;
	}
	
	if (accept2(par, ident, oparen))
	{
		size_t ind = ast_symbolize(par->scope, par->acc.id, 1);
		node_t *new = ast_init_node(call, node);
		parser_expression(par, new);

		/* function invokation */
		if (accept2(par, cparen, semic))
		{
			new->call.sym = ind;
			par->scope->scope.sym[ind].ext = 1;
			return 0;
		}

		return 1;
	}

	if (accept(par, litint) || accept(par, ident))
	{
		switch (par->acc.t)
		{
		case litint:
			ast_init_node(constant, node)->constant.val
				= par->acc.val;
			break;
		case ident:
			ast_init_node(variable, node)->variable.sym =
				ast_symbolize(par->scope, par->acc.id, 0);
			break;
		}
		return 0;
	}

	if (accept(par, plus) || accept(par, minus))
	{
		node_t *new;

		switch (par->acc.t)
		{
		case plus:
			new = ast_init_node(add, node);
			break;
		case minus:
			new = ast_init_node(sub, node);
			break;
		}

		parser_expression(par, new);
		parser_expression(par, new);
		return 0;
	}

	return 1;
}

char parser_scope(parser_t *par, node_t *node)
{
	par->scope = node;

	while (!parser_expression(par, node)) { }

	return 0;
}

