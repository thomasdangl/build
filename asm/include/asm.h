
#ifndef ASM_ASM_H
#define ASM_ASM_H

#include "lexer.h"
#include "op.h"

enum symbol_type
{
	LABEL,
	EXTERN
};

typedef struct
{
	enum symbol_type type;
	char *name;
	size_t addr;
} symbol_t;

typedef struct
{
	symbol_t *sym;
	size_t addr;
} reloc_t;

typedef struct
{
	char *mnemonic;
	char **op;
	size_t op_count;
	symbol_t **op_sym;
	size_t op_sym_count;
} instr_t;

typedef struct
{
	lexer_t *lex;
	instr_t cur;
	char *section;
	char ext;
	char *token;

	char *out;
	size_t out_count;
	size_t last_out_count;

	symbol_t *sym;
	size_t sym_count;
	size_t last_sym_count;

	reloc_t *rel;
	size_t rel_count;
} asm_t;

asm_t* asm_init(lexer_t *lex);
void asm_full_pass(asm_t *as);

void asm_advance(asm_t *as, char *new);
void asm_make_instr(asm_t *as);
char asm_consume_label(asm_t *as);
char asm_consume_extern(asm_t *as);

op_t* asm_match_op(asm_t *as);
void asm_resolve_op(asm_t *as, size_t ind);
char asm_decode_reg(asm_t *as, size_t ind);
long asm_decode_imm(asm_t *as, size_t ind);

void asm_emit(asm_t *as, char byte);
void asm_emit_imm(asm_t *as, size_t op, size_t val);

void asm_emit_current_labels(asm_t *as, char *line);
void asm_emit_current_hex(asm_t *as, char *line);

symbol_t* asm_find_symbol(asm_t *as, const char *name);
symbol_t* asm_iterate_symbols(asm_t *as, size_t ind);
reloc_t* asm_iterate_relocs(asm_t *as, size_t ind);

#endif /* ASM_ASM_H */

