
#include "codegen.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

static const char* systemv_regs[] = { RDI, RSI, RDX, RCX, R8, R9 };
static const char* ar_regs[] = { RAX, RBX, R10, R11, R12, R13, R14, R15 };

static size_t align_by(long long addr, char al)
{
	return (addr + (al - 1)) & -al;
}

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
	codegen_emit(cg, "section .text\n");

	symbol_t *sym;
	for (size_t i = 0; i < cg->ast->scope.sym_count; i++)
	{
		sym = &cg->ast->scope.sym[i];
		if (sym->ext == 1)
			codegen_emit(cg, "extern %s", sym->name);
	}
	if (cg->ast->scope.sym_count > 0)
		codegen_emit(cg, "");

	codegen_emit(cg, "main::");
	cg->indent++;
	codegen_eval_node(cg, cg->ast);
	cg->indent--;
}

void codegen_eval_node(codegen_t *cg, node_t *node)
{
	switch (node->type)
	{
	case scope: codegen_scope(cg, node); break;
	case call: codegen_call(cg, node); break;
	case constant: codegen_constant(cg, node); break;
	case variable: codegen_variable(cg, node); break;
	case assign: codegen_assign(cg, node); break;
	case add: codegen_add(cg, node); break;
	case sub: codegen_sub(cg, node); break;
	default: printf("Unhandled node type `%d` in AST.\n", node->type); exit(1);
	}
}

void codegen_scope(codegen_t *cg, node_t *node)
{
	size_t stack = align_by(node->scope.sym_count * sizeof(long long),
				sizeof(long long));

	/* write procedure prologue. */
	codegen_emit(cg, "PUSH RBP");
	codegen_emit(cg, "MOV RBP, RSP");
	codegen_emit(cg, "SUB RSP, %d\n", stack);

	/* write procedure body. */
	for (size_t i = 0; i < node->children_count; i++)
		codegen_eval_node(cg, node->children[i]);
	
	/* write procedure epilogue. */
	codegen_emit(cg, "");
	codegen_emit(cg, "ADD RSP, %d", stack);
	codegen_emit(cg, "POP RBP");
	codegen_emit(cg, "RET");
}

void codegen_call(codegen_t *cg, node_t *node)
{
	for (size_t i = 0; i < node->children_count; i++)
		if (i < sizeof(systemv_regs) / sizeof(char*))
		{
			switch (node->children[i]->type)
			{
			case constant:
				codegen_emit(cg, "MOV %s, %d", systemv_regs[i],
						node->children[i]->constant.val);
				break;
			case variable:
				codegen_emit(cg, "MOV %s, [RSP-%d]", systemv_regs[i],
						sizeof(long long) * node->children[i]->variable.sym);
				break;
			default:
				/* warning! cannot clobber systemv regs! */
				codegen_eval_node(cg, node->children[i]);
				codegen_emit(cg, "MOV %s, RAX", systemv_regs[i]);
				break;
			}
		}
		else
		{
			printf("Passing arguments on the stack is not implemented!\n");
			exit(1);
		}

	symbol_t *sym = &cg->ast->scope.sym[node->call.sym];
	codegen_emit(cg, "CALL %s", sym->name);
}

void codegen_constant(codegen_t *cg, node_t *node)
{
	if (node->children_count != 0)
	{
		printf("Ill-formated constant.\n");
		exit(1);
	}

	codegen_emit(cg, "MOV %s, %d",
			ar_regs[cg->ar_base],
			node->constant.val);
}

void codegen_variable(codegen_t *cg, node_t *node)
{
	if (node->children_count != 0)
	{
		printf("Ill-formated variable.\n");
		exit(1);
	}

	codegen_emit(cg, "MOV %s, [RSP-%d]",
			ar_regs[cg->ar_base],
			sizeof(long long) * node->variable.sym);
}

void codegen_assign(codegen_t *cg, node_t *node)
{
	if (node->children_count != 2 || node->children[0]->type != variable)
	{
		printf("Ill-formated assignment.\n");
		exit(1);
	}

	if (node->children[1]->type == constant)
	{
		codegen_emit(cg, "MOV [RSP-%d], %d", sizeof(long long)
				* node->children[0]->variable.sym,
				node->children[1]->constant.val);
		return;
	}

	codegen_eval_node(cg, node->children[1]);
	codegen_emit(cg, "MOV [RSP-%d], RAX", sizeof(long long)
			* node->children[0]->variable.sym);
}

/*
 * For arithmetic expressions, see the algorithm
 * in the dragon book (2nd edition) page 569.
 */
static size_t step(node_t *node)
{
	if (node->type == variable || node->type == constant)
		return 1;
	return MAX(step(node->children[0]),
		step(node->children[1])) + 1;
}

static void codegen_ar_expr(codegen_t *cg, node_t *node)
{
	size_t left = step(node->children[0]);
	size_t right = step(node->children[1]);

	if (left == right)
	{
		cg->ar_base++;
		codegen_eval_node(cg, node->children[1]);
		cg->ar_base--;
		codegen_eval_node(cg, node->children[0]);
	}
	else if (left > right)
	{
		cg->ar_base++;
		codegen_eval_node(cg, node->children[0]);
		cg->ar_base--;
		codegen_eval_node(cg, node->children[1]);
	}
	else if (right > left)
	{
		cg->ar_base++;
		codegen_eval_node(cg, node->children[1]);
		cg->ar_base--;
		codegen_eval_node(cg, node->children[0]);
	}
}

void codegen_add(codegen_t *cg, node_t *node)
{
	if (node->children_count != 2)
	{
		printf("Ill-formated add.\n");
		exit(1);
	}

	codegen_ar_expr(cg, node);
	codegen_emit(cg, "ADD %s, %s",
		ar_regs[cg->ar_base],
		ar_regs[cg->ar_base + 1]);
}

void codegen_sub(codegen_t *cg, node_t *node)
{
	if (node->children_count != 2)
	{
		printf("Ill-formated sub.\n");
		exit(1);
	}

	codegen_ar_expr(cg, node);
	codegen_emit(cg, "SUB %s, %s",
		ar_regs[cg->ar_base],
		ar_regs[cg->ar_base + 1]);
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

