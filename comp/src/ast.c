
#include "ast.h"

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
	parent->children = realloc(parent->children, ++parent->children_count * sizeof(node_t));
	parent->children[parent->children_count - 1] = child;
}

