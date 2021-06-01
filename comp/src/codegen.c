
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
	case str_constant: codegen_str_constant(cg, node); break;
	case variable: codegen_variable(cg, node); break;
	case assign: codegen_assign(cg, node); break;
	case add: case sub: case mul: case divi: codegen_ar_expr(cg, node); break;
	case ret: codegen_return(cg, node); break;
	case dbg: codegen_dbg(cg, node); break;
	default: printf("Unhandled node type `%d` in AST.\n", node->type); exit(1);
	}
}

void codegen_scope(codegen_t *cg, node_t *node)
{
	cg->scope = node;
	size_t stack = 0;
	for (size_t i = 0; i < node->scope.sym_count; i++)
		if (!node->scope.sym[i].ext)
			stack += sizeof(long long);
	stack = align_by(stack, 16);

	/* produce other scopes first. */
	for (size_t i = 0; i < node->children_count; i++)
		if (node->children[i]->type == scope)
			codegen_eval_node(cg, node->children[i]);

	/* write procedure prologue. */
	cg->id = "main";
	if (node->scope.parent)
		cg->id = node->scope.parent->scope.sym[node->scope.self].name;
	codegen_emit(cg, "");
	codegen_emit_label(cg, "%s:", cg->id);
	if (stack > 0)
	{
		codegen_emit(cg, "PUSH RBP");
		codegen_emit(cg, "MOV RBP, RSP");
		codegen_emit(cg, "SUB RSP, %d", stack);
	}

	/* adjust calling convention until we have register allocation. */
	for (size_t i = 0; i < node->scope.sym_count; i++)
		if (node->scope.sym[i].arg)
			codegen_emit(cg, "MOV %s, %s",
				codegen_resolve(cg, i),
				systemv_regs[i]);

	/* write procedure body. */
	for (size_t i = 0; i < node->children_count; i++)
		if (node->children[i]->type != scope)
			codegen_eval_node(cg, node->children[i]);
	
	/* write procedure epilogue. */
	codegen_emit(cg, "");
	codegen_emit_label(cg, "_%s_exit", cg->id);
	if (stack > 0)
	{
		codegen_emit(cg, "ADD RSP, %d", stack);
		codegen_emit(cg, "POP RBP");
	}
	codegen_emit(cg, "RET");
	cg->scope = cg->ast;
}

void codegen_call(codegen_t *cg, node_t *node)
{
	symbol_t *sym = &cg->ast->scope.sym[node->call.sym];
	if (sym->vaarg)
		codegen_emit(cg, "MOV RAX, %d", node->children_count);

	char *r;
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
				r = codegen_resolve(cg,	node->children[i]->variable.sym);
				if (strcmp(r, systemv_regs[i]) != 0)
					codegen_emit(cg, "MOV %s, %s", systemv_regs[i], r);
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

void codegen_str_constant(codegen_t *cg, node_t *node)
{
	if (node->children_count != 0)
	{
		printf("Ill-formated constant.\n");
		exit(1);
	}

	codegen_emit(cg, "LEA %s, [strs+%d]", ar_regs[cg->ar_base],
		node->str_constant.offset);
}

void codegen_variable(codegen_t *cg, node_t *node)
{
	if (node->children_count != 0)
	{
		printf("Ill-formated variable.\n");
		exit(1);
	}

	codegen_emit(cg, "MOV %s, %s", ar_regs[cg->ar_base],
		codegen_resolve(cg, node->variable.sym));
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
		codegen_emit(cg, "MOV %s, %d",
			codegen_resolve(cg, node->children[0]->variable.sym),
			node->children[1]->constant.val);
		return;
	}

	codegen_eval_node(cg, node->children[1]);
	codegen_emit(cg, "MOV %s, RAX",
		codegen_resolve(cg, node->children[0]->variable.sym));
}

void codegen_return(codegen_t *cg, node_t *node)
{
	if (node->children_count >= 2)
	{
		printf("Ill-formated return.\n");
		exit(1);
	}

	if (node->children_count > 0)
		codegen_eval_node(cg, node->children[0]);
	codegen_emit(cg, "JMP _%s_exit", cg->id);
}

void codegen_dbg(codegen_t *cg, node_t *node)
{
	if (node->children_count != 0)
	{
		printf("Ill-formated dbg.\n");
		exit(1);
	}

	codegen_emit(cg, "");
	codegen_emit(cg, "# Line: %d", node->dbg.lino);
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
	     c = left <= right,
	     or;

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
		case mul:
			codegen_emit(cg, "IMUL %s, %s",
				ar_regs[0], ar_regs[1]);
			break;
		/* x86 ISA is stupid. */
		case divi:
			or = strcmp(ar_regs[0], RAX) != 0;
			codegen_emit(cg, "PUSH RDX");
			if (or)
			{
				codegen_emit(cg, "PUSH RAX");
				codegen_emit(cg, "MOV RAX, %s", ar_regs[0]);
			}
			codegen_emit(cg, "XOR RDX, RDX");
			codegen_emit(cg, "IDIV %s",
				ar_regs[1]);
			if (or)
			{
				codegen_emit(cg, "MOV %s, RAX", ar_regs[0]);
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
	else
	{
		cg->ar_base++;
		codegen_eval_node(cg, node->children[c ? 1 : 0]);
		cg->ar_base--;
		codegen_eval_node(cg, node->children[c ? 0 : 1]);
	
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

char* codegen_resolve(codegen_t *cg, size_t sym)
{
	/* TODO: add register allocation? */
	char scratch[255] = { 0 };
	sprintf(scratch, "[RSP-%d]", sizeof(long long) * sym);
	return strdup(scratch);
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

