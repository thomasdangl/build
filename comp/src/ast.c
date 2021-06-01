
#include "ast.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define is_odigit(c)  ('0' <= c && c <= '7')

node_t* ast_init_node(op_t type, node_t *parent)
{
	node_t *node = calloc(1, sizeof(node_t));

	if (parent)
		ast_node_insert(parent, node);

	node->type = type;
	return node;
}

void ast_node_insert(node_t *parent, node_t *child)
{
	ast_node_insert_at(parent, child, parent->children_count);
}

void ast_node_insert_at(node_t *parent, node_t *child, size_t pos)
{
	if (pos > parent->children_count)
	{
		printf("Invalid position in node.\n");
		exit(1);
	}

	parent->children = realloc(parent->children,
		++parent->children_count * sizeof(node_t));

	for (size_t i = parent->children_count - 1; i > pos; i--)
		parent->children[i] = parent->children[i - 1];

	parent->children[pos] = child;
}

size_t ast_symbolize(node_t *node, const char *name, char global, char insert)
{
	if (global && node->scope.parent)
		return ast_symbolize(node->scope.parent, name, global, insert);

	for (size_t i = 0; i < node->scope.sym_count; i++)
		if (strcmp(node->scope.sym[i].name, name) == 0)
			return i;

	if (!insert)
		return -1;

	node->scope.sym = realloc(node->scope.sym,
			++node->scope.sym_count * sizeof(symbol_t));
	size_t ind = node->scope.sym_count - 1;
	node->scope.sym[ind] = (symbol_t)
	{
		.name = name
	};
	return ind;
}

static size_t unescape(char *s)
{
	static const char escapes[256] =
	{
		['"'] = '"',
		['\''] = '\'',
		['\\'] = '\\',
		['a'] = '\a',
		['b'] = '\b',
		['E'] = 033,
		['e'] = 033,
		['f'] = '\f',
		['n'] = '\n',
		['r'] = '\r',
		['t'] = '\t',
		['v'] = '\v'
	};
	size_t m, q;
	char *r, *w;

	for (r = w = s; *r;)
	{
		if (*r != '\\')
		{
			*w++ = *r++;
			continue;
		}
		r++;
		if (!*r)
		{
			printf("null escape sequence\n");
			exit(1);
		}
		else if (escapes[(unsigned char)*r])
			*w++ = escapes[(unsigned char)*r++];
		else if (is_odigit(*r))
		{
			for (q = 0, m = 4; m && is_odigit(*r); m--, r++)
				q = q * 8 + (*r - '0');
			*w++ = q <= 255 ? q : 255;
		}
		else if (*r == 'x' && isxdigit(r[1]))
		{
			r++;
			for (q = 0, m = 2; m && isxdigit(*r); m--, r++)
				if (isdigit(*r))
					q = q * 16 + (*r - '0');
				else
					q = q * 16 + (tolower(*r) - 'a' + 10);
			*w++ = q;
		}
		else
		{
			printf("invalid escape sequence '\\%c'\n", *r);
			exit(1);
		}
	}

	*w = '\0';
	return w - s;
}

size_t ast_stringify(node_t *node, const char *str)
{
	if (node->scope.parent)
		return ast_stringify(node->scope.parent, str);

	/* TODO: Implement lookup if we reuse the same string literal. */
	size_t len = strlen(str) + 2, old = node->scope.strs_len;
	node->scope.strs = realloc(node->scope.strs,
			node->scope.strs_len + len + 1);
	strcpy(node->scope.strs + old, str);
	node->scope.strs_len += len;
	node->scope.strs[node->scope.strs_len - 2] = '\\';
	node->scope.strs[node->scope.strs_len - 1] = '0';
	node->scope.strs[node->scope.strs_len] = '\0';
	char *real = strdup(node->scope.strs + old),
	     ind = node->scope.strs_ind;
	unescape(real);
	node->scope.strs_ind += strlen(real) + 1;
	return ind;
}

void ast_print(node_t *node, char indent)
{
	const char *type = 0;
	const size_t size = indent + 5 /* "|-->" + null terminator */;
	char *ind = alloca(size);
	memset(ind, 0, size);

	switch (node->type)
	{
	case scope:
		type = "%sSCOPE@%p";
		break;
	case call:
		type = "%sCALL@%p\n";
		break;
	case constant:
		type = "%sCONST@%p\n";
		break;
	case str_constant:
		type = "%sSTR_CONST@%p\n";
		break;
	case variable:
		type = "%sVAR@%p\n";
		break;
	case assign:
		type = "%sASSIGN@%p\n";
		break;
	case add:
		type = "%sADD@%p\n";
		break;
	case sub:
		type = "%sSUB@%p\n";
		break;
	case mul:
		type = "%sMUL@%p\n";
		break;
	case divi:
		type = "%sDIV@%p\n";
		break;
	case ret:
		type = "%sRET@%p\n";
		break;
	case dbg:
		return;
	default:
		printf("Unhandled node in ast_print %p!\n", node);
		exit(1);
	}

	if (indent > 0)
	{
		ind[indent - 1] = '|';
		ind[indent] = '-';
		ind[indent + 1] = '-';
		ind[indent + 2] = '>';
	}

	for (char i = 0; i < indent - 1; i++)
		ind[i] = '\t';

	printf(type, ind, node);

	if (node->type == scope)
	{
		const char *id = "main";
		if (node->scope.parent)
			id = node->scope.parent->scope
				.sym[node->scope.self].name;
		printf(" [%s] { ", id);
		if (node->scope.sym_count == 0)
			printf(" }\n");
		for (size_t i = 0; i < node->scope.sym_count; i++)
			if (i != node->scope.sym_count - 1)
				printf("%s, ", node->scope.sym[i].name);
			else
				printf("%s }\n", node->scope.sym[i].name);
	}

	for (size_t i = 0; i < node->children_count; i++)
		ast_print(node->children[i], indent + 1);
}

