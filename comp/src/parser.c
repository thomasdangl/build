
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define EXPECT(p, s) if (!accept(p, s)) \
	{ \
		printf("Missing `%s` in line %d.\n", #s, p->lino - 1); \
		exit(1); \
	}
#define EXPECT2(p, s1, s2) if (!accept2(p, s1, s2)) \
	{ \
		printf("Missing `%s` and `%s` in line %d.\n", \
			#s1, #s2, p->lino - 1); \
		exit(1); \
	}

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

	par->lino++;
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
	case '\n': 
	{
		par->lino++;
		node_t *new = ast_init_node(dbg, 0);
		new->dbg.lino = par->lino;
		if (par->scope->children_count > 0 && par->scope->children
			[par->scope->children_count - 1]->type != scope)
		{
			if (par->dbgp == par->ctl)
			{
				if (par->dbgc < par->ctl->children_count)
					ast_node_insert_at(par->dbgp, new, par->dbgc);
			}
			else if (par->ctl->type == scope)
				ast_node_insert_at(par->ctl, new, 0);
		}
		par->dbgp = par->ctl;
		par->dbgc = par->ctl->children_count;
	}
	case '\t': case '\r': case ' ': goto start;
	case '\0': par->lah.t = eof; par->cur = 0; return;
	case '(' : par->lah.t = oparen; return;
	case ')' : par->lah.t = cparen; return;
	case '{' : par->lah.t = ocurl; return;
	case '}' : par->lah.t = ccurl; return;
	case ',' : par->lah.t = comma; return;
	case ';' : par->lah.t = semic; return;
	case '=' : par->lah.t = equals; return;
	case '+' : par->lah.t = plus; return;
	case '-' : par->lah.t = minus; return;
	case '<' : par->lah.t = lt; return;
	case '>' : par->lah.t = gt; return;
	case '*' : par->lah.t = times; return;
	case '~' : par->lah.t = neg; return;
	case '\"':
	{
		const char *start = par->cur + 1;
		do
		{
			par->cur++;
		} while (*par->cur != '\"' || *(par->cur - 1) == '\\');

		par->lah.t = quot;
		par->lah.id = strndup(start, par->cur - start);
		return;
	}
	case '/' : 
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
			par->lah.t = frac;
			return;
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

		if (!strcmp(par->lah.id, "if"))
			par->lah.t = ifc;
		else if (!strcmp(par->lah.id, "else"))
			par->lah.t = elsec;
		else if (alpha)
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

char parser_lexpression(parser_t *par, node_t *node)
{
	if (!parser_if(par, node))
		return 0;

	if (!parser_eq(par, node))
		return 0;
	
	if (!parser_call(par, node))
	{
		EXPECT(par, semic);
		return 0;
	}

	if (!parser_return(par, node))
		return 0;

	return 1;
}

char parser_rexpression(parser_t *par, node_t *node)
{
	if (!parser_call(par, node))
		return 0;

	if (!parser_val(par, node))
		return 0;

	if (!parser_arith(par, node))
		return 0;

	return 1;
}

char parser_scope(parser_t *par, node_t *node)
{
	do
	{
		par->scope = par->ctl = node;
	} while (!parser_lexpression(par, node));

	return 0;
}

char parser_eq(parser_t *par, node_t *node)
{
	if (accept2(par, ident, equals))
	{
		char *id = par->acc.id;
		if (par->scope == par->ast && accept(par, oparen))
			return parser_fn(par, node, id);
		else
			return parser_assign(par, node);
	}

	return 1;
}

char parser_assign(parser_t *par, node_t *node)
{
	node_t *new = ast_init_node(assign, node);
	ast_init_node(variable, new)->variable.sym =
		ast_symbolize(par->scope, par->acc.id, 0, 1);
	if (parser_rexpression(par, new))
	{
		printf("Fatal error when constructing assignment.\n");
		exit(1);
	}

	return !accept(par, semic);
}

char parser_fn(parser_t *par, node_t *node, char *id)
{
	node_t *new = ast_init_node(scope, 0);

	while (accept(par, ident))
	{
		size_t sym = ast_symbolize(new, par->acc.id, 0, 1);
		new->scope.sym[sym].arg = 1;

		if (!accept(par, comma))
			break;
	}

	if (accept2(par, cparen, ocurl))
	{
		size_t ind = ast_symbolize(par->scope, id, 1, 1);
		new->scope.self = ind;
		new->scope.parent = par->scope;

		if (parser_scope(par, new))
		{
			printf("Fatal error when parsing scope of fn.\n");
			exit(1);
		}

		EXPECT(par, ccurl);
		ast_node_insert(node, new);
		return 0;
	}

	return 1;
}

char parser_if(parser_t *par, node_t *node)
{
	if (accept2(par, ifc, oparen))
	{
		node_t *ifn = ast_init_node(ifo, node);
		par->ctl = ifn;

		if (parser_rexpression(par, ifn))
		{
			printf("No condition in if clause.\n");
			exit(1);
		}

		EXPECT2(par, cparen, ocurl);
		while (!parser_lexpression(par, ifn)) { }
		EXPECT(par, ccurl);
		ifn->ifo.el = ifn->children_count;

		if (accept2(par, elsec, ocurl))
		{
			while (!parser_lexpression(par, ifn)) { }
			EXPECT(par, ccurl);
		}
		par->ctl = par->dbgp = par->scope;
		par->dbgc = par->scope->children_count;
	}

	return 1;
}

char parser_call(parser_t *par, node_t *node)
{
	if (accept2(par, ident, oparen) || accept2(par, ident, neg))
	{
		size_t ind = ast_symbolize(par->scope, par->acc.id, 1, 1);
		if (par->acc2.t == neg)
		{
			par->ast->scope.sym[ind].vaarg = 1;
			EXPECT(par, oparen);
		}

		node_t *new = ast_init_node(call, node);
		while (!parser_rexpression(par, new))
			if (!accept(par, comma))
				break;

		/* function invokation */
		EXPECT(par, cparen);
		new->call.sym = ind;
		par->ast->scope.sym[ind].ext = 1;
		return 0;
	}

	return 1;
}

char parser_val(parser_t *par, node_t *node)
{
	if (accept(par, litint) || accept(par, ident) || accept(par, quot))
	{
		switch (par->acc.t)
		{
		case litint:
			ast_init_node(constant, node)->constant.val
				= par->acc.val;
			break;
		case ident:
		{
			size_t sym = ast_symbolize(par->scope, par->acc.id, 0, 0);

			if (sym == -1)
			{
				printf("Attempted to use variable "
					"before it was declared.\n");
				exit(1);
			}

			ast_init_node(variable, node)->variable.sym = sym;
			break;
		}
		case quot:
			ast_init_node(str_constant, node)
				->str_constant.offset =
				ast_stringify(par->scope, par->acc.id);
			break;
		}
		return 0;
	}

	return 1;
}

char parser_arith(parser_t *par, node_t *node)
{
	if (accept(par, plus) || accept(par, minus)
		|| accept(par, times) || accept(par, frac))
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
		case times:
			new = ast_init_node(mul, node);
			break;
		case frac:
			new = ast_init_node(divi, node);
			break;
		}

		if (parser_rexpression(par, new)
			|| parser_rexpression(par, new))
		{
			printf("Fatal error when constructing arith.\n");
			exit(1);
		}
		return 0;
	}

	return 1;
}

char parser_return(parser_t *par, node_t *node)
{
	if (accept2(par, minus, gt))
	{
		parser_rexpression(par, ast_init_node(ret, node));
		EXPECT(par, semic);
		return 0;
	}

	return 1;
}

#undef EXPECT
#undef EXPECT2

