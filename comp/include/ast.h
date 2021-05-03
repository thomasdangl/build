
#ifndef COMP_AST_H
#define COMP_AST_H

#include <stdlib.h>

typedef enum
{
	scope, call, constant, variable, assign, add, sub
} op_t;

typedef struct
{
	const char *name;
} symbol_t;

typedef struct node
{
	op_t type;
	struct node **children;
	size_t children_count;

	union
	{
		struct
		{
			symbol_t *sym;
			size_t sym_count;
		} scope;

		struct
		{
			/* TODO: this should refer to the symbol table */
			const char *name;
		} call;

		struct
		{
			long long val;
		} constant;
		
		struct
		{
			size_t sym;
		} variable;
	};
} node_t;

node_t* ast_init_node(op_t type, node_t *parent);
void ast_node_insert(node_t *parent, node_t *child);

size_t ast_symbolize(node_t *node, const char *name);
void ast_print(node_t *node, char indent);

#endif /* COMP_AST_H */

