
#ifndef COMP_AST_H
#define COMP_AST_H

#include <stdlib.h>

typedef enum
{
	nop, call, constant
} op_t;

typedef struct node
{
	op_t type;
	struct node **children;
	size_t children_count;

	union
	{
		struct
		{
			/* TODO: this should refer to the symbol table */
			const char *name;
		} call;

		struct
		{
			long long val;
		} constant;
	};
} node_t;

node_t* ast_init_node(op_t type, node_t *parent);
void ast_node_insert(node_t *parent, node_t *child);

#endif /* COMP_AST_H */

