
#include "ast.h"
#include <stdio.h>
#include <string.h>

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
	parent->children = realloc(parent->children,
			++parent->children_count * sizeof(node_t));
	parent->children[parent->children_count - 1] = child;
}

size_t ast_symbolize(node_t *node, const char *name, char insert)
{
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

size_t ast_stringify(node_t *node, const char *str)
{
	/* TODO: Implement lookup if we reuse the same string literal. */
	size_t len = strlen(str) + 2, old = node->scope.strs_len;
	node->scope.strs = realloc(node->scope.strs,
			node->scope.strs_len + len);
	strcpy(node->scope.strs + old, str);
	node->scope.strs_len += len;
	node->scope.strs[node->scope.strs_len - 2] = '\\';
	node->scope.strs[node->scope.strs_len - 1] = '0';
	return old - (old > 0);
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
	case retn:
		type = "%sRETN@%p\n";
		break;
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
		printf(" { ");
		for (size_t i = 0; i < node->scope.sym_count; i++)
			if (i != node->scope.sym_count - 1)
				printf("%s, ", node->scope.sym[i].name);
			else
				printf("%s }\n", node->scope.sym[i].name);
	}

	for (size_t i = 0; i < node->children_count; i++)
		ast_print(node->children[i], indent + 1);
}

