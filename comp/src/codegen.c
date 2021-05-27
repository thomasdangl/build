
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
	if (cg->ast->scope.strs_len > 0)
	{
		codegen_emit(cg, "section .data\n");
		codegen_emit(cg, "strs: db _\"%s\"", cg->ast->scope.strs);
		codegen_emit(cg, "");
	}

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
	case add: case sub: case mul: case divi: codegen_ar_expr(cg, node); break;
	case retn: codegen_return(cg, node); break;
	default: printf("Unhandled node type `%d` in AST.\n", node->type); exit(1);
	}
}

void codegen_scope(codegen_t *cg, node_t *node)
{
	/* TODO: correct this for the number of local variables. */
	size_t stack = align_by(node->scope.sym_count * sizeof(long long), 16);

	/* produce other scopes first. */
	for (size_t i = 0; i < node->children_count; i++)
		if (node->children[i]->type == scope)
			codegen_eval_node(cg, node->children[i]);

	/* write procedure prologue. */
	const char *id = "main";
	if (node->scope.parent)
		id = node->scope.parent->scope.sym[node->scope.self].name;
	codegen_emit_label(cg, "%s:", id);
	codegen_emit(cg, "PUSH RBP");
	codegen_emit(cg, "MOV RBP, RSP");
	codegen_emit(cg, "SUB RSP, %d\n", stack);

	/* write procedure body. */
	for (size_t i = 0; i < node->children_count; i++)
		if (node->children[i]->type != scope)
			codegen_eval_node(cg, node->children[i]);
	
	/* write procedure epilogue. */
	codegen_emit_label(cg, "_%s_exit", id);
	codegen_emit(cg, "ADD RSP, %d", stack);
	codegen_emit(cg, "POP RBP");
	codegen_emit(cg, "RET");
}

void codegen_call(codegen_t *cg, node_t *node)
{
	/* TODO: Implement special case for va_args. */
	codegen_emit(cg, "MOV RAX, %d", node->children_count);
	
	for (size_t i = 0; i < node->children_count; i++)
		if (i < sizeof(systemv_regs) / sizeof(char*))
		{
			switch (node->children[i]->type)
			{
			case constant:
				codegen_emit(cg, "MOV %s, %d", systemv_regs[i],
					node->children[i]->constant.val);
				break;
			case str_constant:
				codegen_emit(cg, "LEA %s, [strs+%d]", systemv_regs[i],
					node->children[i]->str_constant.offset);
				break;
			case variable:
				codegen_emit(cg, "MOV %s, [RSP-%d]", systemv_regs[i],
					sizeof(long long) *
					node->children[i]->variable.sym);
				break;
			default:
				/* warning! cannot clobber systemv regs! */
				codegen_eval_node(cg, node->children[i]);
				codegen_emit(cg, "MOV %s, RAX",
					systemv_regs[i]);
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

void codegen_return(codegen_t *cg, node_t *node)
{
	if (node->children_count != 1)
	{
		printf("Ill-formated return.\n");
		exit(1);
	}

	codegen_eval_node(cg, node->children[0]);
	codegen_emit(cg, "JMP exit");
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

void codegen_ar_expr(codegen_t *cg, node_t *node)
{
	if (node->children_count != 2)
	{
		printf("Ill-formated ar_expr.\n");
		exit(1);
	}

	size_t left = step(node->children[0]),
	       right = step(node->children[1]),
	       s = sizeof(ar_regs) / sizeof(char*);

	char lgtr = left >= s,
	     rgtr = right >= s,
	     c = left <= right;

	if (lgtr || rgtr)
	{
		char gtr = c ? lgtr : rgtr;
		cg->ar_base = 0;
		codegen_eval_node(cg, node->children[c ? 1 : 0]);
		codegen_emit(cg, "PUSH %s", ar_regs[0]);
		cg->ar_base = gtr ? 0 : (c ? left : right);
		codegen_eval_node(cg, node->children[c ? 0 : 1]);
		codegen_emit(cg, "POP %s", ar_regs[0]);
		switch (node->type)
		{
		case add:
			codegen_emit(cg, "ADD %s, %s",
				ar_regs[0], ar_regs[1]);
			break;
		case sub:
			codegen_emit(cg, "SUB %s, %s",
				ar_regs[0], ar_regs[1]);
			break;
		default:
			printf("Unhandled type `%d` in ar_expr.\n",
				node->type);
			exit(1);
		}
	}
	else
	{
		cg->ar_base++;
		codegen_eval_node(cg, node->children[c ? 1 : 0]);
		cg->ar_base--;
		codegen_eval_node(cg, node->children[c ? 0 : 1]);
	
		char or;	
		switch (node->type)
		{
		case add:
			codegen_emit(cg, "ADD %s, %s",
				ar_regs[cg->ar_base],
				ar_regs[cg->ar_base + 1]);
			break;
		case sub:
			codegen_emit(cg, "SUB %s, %s",
				ar_regs[cg->ar_base],
				ar_regs[cg->ar_base + 1]);
			break;
		case mul:
			codegen_emit(cg, "IMUL %s, %s",
				ar_regs[cg->ar_base],
				ar_regs[cg->ar_base + 1]);
			break;
		/* x86 ISA is stupid. */
		case divi:
			or = strcmp(ar_regs[cg->ar_base], RAX) != 0;
			codegen_emit(cg, "PUSH RDX");
			if (or)
			{
				codegen_emit(cg, "PUSH RAX");
				codegen_emit(cg, "MOV RAX, %s", ar_regs[cg->ar_base]);
			}
			codegen_emit(cg, "XOR RDX, RDX");
			codegen_emit(cg, "IDIV %s",
				ar_regs[cg->ar_base + 1]);
			if (or)
			{
				codegen_emit(cg, "MOV %s, RAX", ar_regs[cg->ar_base]);
				codegen_emit(cg, "POP RAX");
			}
			codegen_emit(cg, "POP RDX");
			break;
		default:
			printf("Unhandled type `%d` in ar_expr.\n",
				node->type);
			exit(1);
		}
	}
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

void codegen_emit_label(codegen_t *cg, const char *label, ...)
{
	va_list args;
	va_start(args, label);
	cg->cur += vsprintf(cg->cur, label, args);
	*cg->cur = ':'; cg->cur++;
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

