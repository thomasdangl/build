
#ifndef COMP_AST_H
#define COMP_AST_H

#include <stdlib.h>

typedef enum
{
	scope, call, constant, str_constant, variable, assign,
	add, sub, mul, divi, ret, dbg, ifo, eq
} op_t;

typedef struct
{
	const char *name;
	char ext;
	char vaarg;
	char arg;
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
			struct node *parent;
			size_t self;

			symbol_t *sym;
			size_t sym_count;
			char *strs;
			size_t strs_len;
			size_t strs_ind;

			size_t ifs;
		} scope;

		struct
		{
			size_t sym;
		} call;
		
		struct
		{
			long long val;
		} constant;

		struct
		{
			size_t offset;
		} str_constant;
		
		struct
		{
			size_t sym;
		} variable;

		struct
		{
			size_t lino;
		} dbg;

		struct
		{
			size_t el;	
		} ifo;
	};
} node_t;

node_t* ast_init_node(op_t type, node_t *parent);
void ast_node_insert(node_t *parent, node_t *child);
void ast_node_insert_at(node_t *parent, node_t *child, size_t pos);

size_t ast_symbolize(node_t *node, const char *name, char global, char insert);
size_t ast_stringify(node_t *node, const char *str);
void ast_print(node_t *node, char indent);

#endif /* COMP_AST_H */

