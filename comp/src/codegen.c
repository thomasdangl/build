
#include "codegen.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* systemv_regs[] = { RDI, RSI, RDX, RCX, R8, R9 };

codegen_t* codegen_init(node_t *ast)
{
	codegen_t *cg = calloc(1, sizeof(codegen_t));
	cg->ast = ast;
	cg->scratch = cg->cur = calloc(0x100000, 1);
	codegen_run(cg);
	return cg;
}

void codegen_run(codegen_t *cg)
{
	codegen_emit(cg, "section .text\n\nextern exit\n\nmain::");
	cg->indent++;
	codegen_eval_node(cg, cg->ast);
	cg->indent--;
}

void codegen_eval_node(codegen_t *cg, node_t *node)
{
	switch (node->type)
	{
	case nop: codegen_nop(cg, node); break;
	case call: codegen_call(cg, node); break;
	case constant: codegen_constant(cg, node); break;
	default: printf("Unhandled node type `%d` in AST.\n", node->type); exit(1);
	}
}

void codegen_nop(codegen_t *cg, node_t *node)
{
	/* nop is used for the program scope */
	for (size_t i = 0; i < node->children_count; i++)
		codegen_eval_node(cg, node->children[i]);
}

void codegen_call(codegen_t *cg, node_t *node)
{
	/* TODO: these need to be evaluted first */
	for (size_t i = 0; i < node->children_count; i++)
		if (i < sizeof(systemv_regs) / sizeof(char*))
		{
			if (node->children[i]->type == constant)
				codegen_emit(cg, "MOV %s, %d", systemv_regs[i],
						node->children[i]->constant.val);
			else
			{
				printf("Passing non-constant arguments is not implemented!\n");
				exit(1);
			}
		}
		else
		{
			printf("Passing arguments on the stack is not implemented!\n");
			exit(1);
		}

	/* TODO: this needs to be hooked up to the symbol table! */
	codegen_emit(cg, "CALL %s", node->call.name);
}

void codegen_constant(codegen_t *cg, node_t *node)
{
	/* TODO: ??? */
}

void codegen_emit(codegen_t *cg, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	for (char i = 0; i < cg->indent; i++)
		*cg->cur++ = '\t';
	cg->cur += vsprintf(cg->cur, format, args);
	*cg->cur = '\n'; cg->cur++;
	va_end(args);
}

void codegen_pretty_print(codegen_t *cg)
{
	char *tmp = strdup(cg->scratch), *p, *op;
	size_t i = 0;
	p = op = strtok(tmp, "\n");
	do
	{
		for (char* j = op; j < p; j++)
			if (*j == '\n')
				printf("%-6d\n", ++i);
		op = p;
		printf("%-6d\t%s\n", ++i, p);
	} while (p = strtok(0, "\n"));
	free(tmp);
}

