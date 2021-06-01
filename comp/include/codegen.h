
#ifndef COMP_CODEGEN_H
#define COMP_CODEGEN_H

#include "ast.h"

#define RAX "RAX"
#define RBX "RBX"
#define RBP "RBP"
#define RCX "RCX"
#define RDX "RDX"
#define RDI "RDI"
#define RSI "RSI"
#define RSP "RSP"
#define R8 "R8"
#define R9 "R9"
#define R10 "R10"
#define R11 "R11"
#define R12 "R12"
#define R13 "R13"
#define R14 "R14"
#define R15 "R15"

typedef struct
{
	node_t *ast, *scope;
	char *scratch, *cur, indent, ar_base;
	const char *id;
} codegen_t;

codegen_t* codegen_init(node_t *ast);
void codegen_run(codegen_t *cg);

void codegen_eval_node(codegen_t *cg, node_t *node);
void codegen_scope(codegen_t *cg, node_t *node);
void codegen_call(codegen_t *cg, node_t *node);
void codegen_constant(codegen_t *cg, node_t *node);
void codegen_str_constant(codegen_t *cg, node_t *node);
void codegen_variable(codegen_t *cg, node_t *node);
void codegen_assign(codegen_t *cg, node_t *node);
void codegen_return(codegen_t *cg, node_t *node);
void codegen_dbg(codegen_t *cg, node_t *node);

void codegen_ar_expr(codegen_t *cg, node_t *node);
char* codegen_resolve(codegen_t *cg, size_t sym);

void codegen_emit(codegen_t *cg, const char *format, ...);
void codegen_emit_label(codegen_t *cg, const char *label, ...);
void codegen_pretty_print(codegen_t *cg);

#endif /* COMP_CODEGEN_H */

