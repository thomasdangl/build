
#include "asm.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#define is_odigit(c)  ('0' <= c && c <= '7')

	/* mn | Op/En | REX long | OP1 | OP2 | CODE_PRIMARY | CODE_SECONDARY | CODE_EXTENSION */
op_t op[] =
{
	/*
	 * NOTE: we do not explicitly pick the smallest possible instruction,
	 * rather, we pick the first instruction that fits all operands.
	 * to produce the best possible output, the instructions below should
	 * be ordered by ascending instruction size.
	 */

	/* LEA — Load Effective Address */
	{ "lea", RM, FALSE, REG16, IMM16, 0x8D, EMPTY, EMPTY },
	{ "lea", RM, FALSE, REG32, IMM32, 0x8D, EMPTY, EMPTY },
	{ "lea", RM, FALSE, REG64, IMM32, 0x8D, EMPTY, EMPTY },

	/* MOV - Move */
	{ "mov", OI, FALSE, REG8, IMM8, 0xB0, EMPTY, EMPTY },
	{ "mov", OI, FALSE, REG32, IMM32, 0xB8, EMPTY, EMPTY },
	{ "mov", MI, FALSE, REG64, IMM32, 0xC7, EMPTY, EMPTY },
	{ "mov", OI, FALSE, REG64, IMM64, 0xB8, EMPTY, EMPTY },
	{ "mov", MR, FALSE, REG64, REG64, 0x89, EMPTY, EMPTY },
	{ "mov", RM, FALSE, REG64, REG64, 0x8B, EMPTY, EMPTY },

	/* PUSH — Push Word, Doubleword or Quadword Onto the Stack */
	{ "push", O, TRUE, REG64, EMPTY, 0x50, EMPTY, EMPTY },
	{ "push", I, FALSE, IMM32, EMPTY, 0x68, EMPTY, EMPTY },

	/* POP — Pop a Value from the Stack */
	{ "pop", O, TRUE, REG64, EMPTY, 0x58, EMPTY, EMPTY },

	/* ADD — Add */
	{ "add", MI, FALSE, REG8, IMM8, 0x80, EMPTY, EMPTY },
	{ "add", MI, FALSE, REG32, IMM8, 0x83, EMPTY, EMPTY },
	{ "add", MI, FALSE, REG64, IMM8, 0x83, EMPTY, EMPTY },
	{ "add", MI, FALSE, REG64, IMM32, 0x81, EMPTY, EMPTY },
	{ "add", MR, FALSE, REG8, REG8, 0x00, EMPTY, EMPTY },
	{ "add", MR, FALSE, REG16, REG16, 0x01, EMPTY, EMPTY },
	{ "add", MR, FALSE, REG32, REG32, 0x01, EMPTY, EMPTY },
	{ "add", MR, FALSE, REG64, REG64, 0x01, EMPTY, EMPTY },
	{ "add", RM, FALSE, REG64, REG64, 0x03, EMPTY, EMPTY },

	/* INC — Increment by 1 */
	{ "inc", M, FALSE, REG64, EMPTY, 0xFF, EMPTY, EMPTY },

	/* IMUL - Signed Multiply */
	{ "imul", RM, FALSE, REG32, REG32, 0x0F, 0xAF, EMPTY },
	{ "imul", RM, FALSE, REG64, REG64, 0x0F, 0xAF, EMPTY },

	/* IDIV - Signed Divide */
	{ "idiv", M, FALSE, REG32, EMPTY, 0xF7, EMPTY, 0x07 },
	{ "idiv", M, FALSE, REG64, EMPTY, 0xF7, EMPTY, 0x07 },
	
	/* SUB — Subtract */
	{ "sub", MI, FALSE, REG8, IMM8, 0x80, EMPTY, 0x05 },
	{ "sub", MI, FALSE, REG32, IMM8, 0x83, EMPTY, 0x05 },
	{ "sub", MI, FALSE, REG64, IMM8, 0x83, EMPTY, 0x05 },
	{ "sub", MI, FALSE, REG64, IMM32, 0x81, EMPTY, 0x05 },
	{ "sub", MR, FALSE, REG8, REG8, 0x28, EMPTY, EMPTY },
	{ "sub", MR, FALSE, REG16, REG16, 0x29, EMPTY, EMPTY },
	{ "sub", MR, FALSE, REG32, REG32, 0x29, EMPTY, EMPTY },
	{ "sub", MR, FALSE, REG64, REG64, 0x29, EMPTY, EMPTY },
	{ "sub", RM, FALSE, REG64, REG64, 0x2B, EMPTY, EMPTY },

	/* DEC — Decrement by 1 */
	{ "dec", M, FALSE, REG64, EMPTY, 0xFF, EMPTY, 0x01 },

	/* XOR — Logical Exclusive OR */
	{ "xor", MR, FALSE, REG8, REG8, 0x30, EMPTY, EMPTY },
	{ "xor", MR, FALSE, REG32, REG32, 0x31, EMPTY, EMPTY },
	{ "xor", MR, FALSE, REG64, REG64, 0x31, EMPTY, EMPTY },
	{ "xor", MI, FALSE, REG8, IMM8, 0x80, EMPTY, 0x06 },
	{ "xor", MI, FALSE, REG16, IMM16, 0x81, EMPTY, 0x06 },
	{ "xor", MI, FALSE, REG32, IMM32, 0x81, EMPTY, 0x06 },
	{ "xor", MI, FALSE, REG64, IMM32, 0x81, EMPTY, 0x06 },

	/* CMP — Compare Two Operands */
	{ "cmp", MI, FALSE, REG64, IMM8, 0x83, EMPTY, 0x07 },
	{ "cmp", MI, FALSE, REG64, IMM32, 0x81, EMPTY, 0x07 },

	/* TEST — Logical Compare */
	{ "test", MI, FALSE, REG64, IMM32, 0xF7, EMPTY, EMPTY },

	/* JMP - Jump */
	{ "jmp", D, FALSE, IMM32, EMPTY, 0xE9, EMPTY, EMPTY },

	/* Jcc — Jump if Condition Is Met */
	{ "je", D, FALSE, IMM32, EMPTY, 0x0F, 0x84, EMPTY },
	{ "jne", D, FALSE, IMM32, EMPTY, 0x0F, 0x85, EMPTY },

	/* INT n/INTO/INT3/INT1 — Call to Interrupt Procedure */
	{ "int", I, FALSE, IMM8, EMPTY, 0xCD, 0x00, EMPTY },

	/* SYSCALL — Fast System Call */
	{ "syscall", ZO, FALSE, EMPTY, EMPTY, 0x0F, 0x05, EMPTY },

	/* CALL — Call Procedure */
	{ "call", D, FALSE, IMM32, EMPTY, 0xE8, EMPTY, EMPTY },

	/* RET — Return from Procedure */
	{ "ret", ZO, FALSE, EMPTY, EMPTY, 0xC3, EMPTY, EMPTY },
	{ "ret", I, FALSE, IMM16, EMPTY, 0xC2, EMPTY, EMPTY },

	/* Pseudo-operations */
	{ "db", EMPTY, FALSE, IMM8, EMPTY, EMPTY, EMPTY, EMPTY }
};

reg_t reg[] =
{
	{ "rax", RAX, REG64, FALSE, FALSE },
	{ "eax", RAX, REG32, FALSE, FALSE },
	{ "ax", RAX, REG16, FALSE, FALSE },
	{ "al", RAX, REG8, FALSE, FALSE },
	{ "ah", RAX, REG8, FALSE, TRUE },
	{ "rcx", RCX, REG64, FALSE, FALSE },
	{ "ecx", RCX, REG32, FALSE, FALSE },
	{ "cx", RCX, REG16, FALSE, FALSE },
	{ "cl", RCX, REG8, FALSE, FALSE },
	{ "ch", RCX, REG8, FALSE, TRUE },
	{ "rdx", RDX, REG64, FALSE, FALSE },
	{ "edx", RDX, REG32, FALSE, FALSE },
	{ "dx", RDX, REG16, FALSE, FALSE },
	{ "dl", RDX, REG8, FALSE, FALSE },
	{ "dh", RDX, REG8, FALSE, TRUE },
	{ "rbx", RBX, REG64, FALSE, FALSE },
	{ "ebx", RBX, REG32, FALSE, FALSE },
	{ "bx", RBX, REG16, FALSE, FALSE },
	{ "bl", RBX, REG8, FALSE, FALSE },
	{ "bh", RBX, REG8, FALSE, TRUE },
	{ "rsp", RSP, REG64, FALSE, FALSE },
	{ "esp", RSP, REG32, FALSE, FALSE },
	{ "sp", RSP, REG16, FALSE, FALSE },
	{ "spl", RSP, REG8, FALSE, FALSE },
	{ "rbp", RBP, REG64, FALSE, FALSE },
	{ "ebp", RBP, REG32, FALSE, FALSE },
	{ "bp", RBP, REG16, FALSE, FALSE },
	{ "bpl", RBP, REG8, FALSE, FALSE },
	{ "rsi", RSI, REG64, FALSE, FALSE },
	{ "esi", RSI, REG32, FALSE, FALSE },
	{ "si", RSI, REG16, FALSE, FALSE },
	{ "sil", RSI, REG8, FALSE, FALSE },
	{ "rdi", RDI, REG64, FALSE, FALSE },
	{ "edi", RDI, REG32, FALSE, FALSE },
	{ "di", RDI, REG16, FALSE, FALSE },
	{ "dil", RDI, REG8, FALSE, FALSE },
	{ "r8", R8, REG64, TRUE, FALSE },
	{ "r8d", R8, REG32, TRUE, FALSE },
	{ "r8w", R8, REG16, TRUE, FALSE },
	{ "r8b", R8, REG8, TRUE, FALSE },
	{ "r9", R9, REG64, TRUE, FALSE },
	{ "r9d", R9, REG32, TRUE, FALSE },
	{ "r9w", R9, REG16, TRUE, FALSE },
	{ "r9b", R9, REG8, TRUE, FALSE },
	{ "r10", R10, REG64, TRUE, FALSE },
	{ "r10d", R10, REG32, TRUE, FALSE },
	{ "r10w", R10, REG16, TRUE, FALSE },
	{ "r10b", R10, REG8, TRUE, FALSE },
	{ "r11", R11, REG64, TRUE, FALSE },
	{ "r11d", R11, REG32, TRUE, FALSE },
	{ "r11w", R11, REG16, TRUE, FALSE },
	{ "r11b", R11, REG8, TRUE, FALSE },
	{ "r12", R12, REG64, TRUE, FALSE },
	{ "r12d", R12, REG32, TRUE, FALSE },
	{ "r12w", R12, REG16, TRUE, FALSE },
	{ "r12b", R12, REG8, TRUE, FALSE },
	{ "r13", R13, REG64, TRUE, FALSE },
	{ "r13d", R13, REG32, TRUE, FALSE },
	{ "r13w", R13, REG16, TRUE, FALSE },
	{ "r13b", R13, REG8, TRUE, FALSE },
	{ "r14", R14, REG64, TRUE, FALSE },
	{ "r14d", R14, REG32, TRUE, FALSE },
	{ "r14w", R14, REG16, TRUE, FALSE },
	{ "r14b", R14, REG8, TRUE, FALSE },
	{ "r15", R15, REG64, TRUE, FALSE },
	{ "r15d", R15, REG32, TRUE, FALSE },
	{ "r15w", R15, REG16, TRUE, FALSE },
	{ "r15b", R15, REG8, TRUE, FALSE }
};

asm_t* asm_init(lexer_t *lex)
{
	asm_t *as =  calloc(1, sizeof(asm_t));
	as->lex = lex;
	as->out_count = as->last_out_count = 0;
	return as;
}

void asm_full_pass(asm_t *as)
{
	char new = 2;
	char line[256] = { 0 };
	size_t loc = -1;

	while (as->token = lexer_advance(as->lex))
	{
		/* print all locs we skipped. */
		size_t new_loc = lexer_loc(as->lex);
		for (size_t i = loc + 1; i < new_loc; i++)
			printf("%d\n", i);
		loc = new_loc;

		/* advance the current state. */
		asm_advance(as, &new);

		/* write out the tokens, one after the other. */
		if (new == 3)
			new = 2;
		else if (new == 2)
		{
			sprintf(line, "%s", as->token);
			new = 1;
		}
		else if (new == 1)
		{
			sprintf(line, "%s %s", strdup(line), as->token);
			new = 0;
		}
		else
			sprintf(line, "%s, %s", strdup(line), as->token);

		/* finished line, print out total op's emitted and new labels. */
		if (!lexer_peek(as->lex))
		{
			asm_emit_current_hex(as, line);
			asm_emit_current_labels(as, line);
			printf("%-6d%s\n", loc, line);
			memset(line, '\0', sizeof(line));
			new = 2;
		}
	}

	/* finalize the last section */
	asm_close_section(as);

	/* there might be even more empty locs with no token for the lexer to catch. */
	for (size_t i = loc + 1; i < as->lex->loc_count; i++)
		printf("%d\n", i);
}

void asm_advance(asm_t *as, char *new)
{
	if (asm_consume_label(as) || asm_consume_extern(as))
	{
		*new = 3;
		return;
	}

	if (strcmp(as->token, "section") == 0)
	{
		asm_close_section(as);
		*new = 3;
		return;
	}

	if (!as->section)
	{
		as->section = as->token;
		as->section_start = as->out_count;
		*new = 3;
		return;
	}

	if (!as->cur.mnemonic)
		as->cur.mnemonic = as->token;
	else
	{
		as->cur.op = realloc(as->cur.op, ++as->cur.op_count * sizeof(dec_t));
		as->cur.op[as->cur.op_count - 1] = (dec_t)
		{
			.op = as->token,
			.sym = 0,
			.disp = ~0,
			.rel = 0,
			.extended = 0,
			.legacy = 0,
			.sub = 0,
			.sub_count = 0
		};
	}

	if (!lexer_peek(as->lex))
		asm_make_instr(as);
}

void asm_make_instr(asm_t *as)
{
	op_t* op = asm_match_op(as);
	
	if (!op)
	{
		printf("Failed to match current instruction to opcode.\n"
			"Mnemonic: %s\nOP count: %d\n", as->cur.mnemonic,
			as->cur.op_count);
		for (size_t i = 0; i < as->cur.op_count; i++)
			printf("OP %d: %s\n", i, as->cur.op[i].op);
		exit(1);
	}

	symbol_t *sym;
	enum operand_encoding_type e = op->op;
	
	/* handle pseudo-instructions first */
	if (e == EMPTY)
	{
		for (size_t i = 0; i < as->cur.op_count; i++)
			for (size_t j = 0; j < as->cur.op[i].sub_count; j++)
				asm_emit_imm(as, op->op_1, asm_decode_imm(as, i, j));
		memset(&as->cur, 0, sizeof(instr_t));
		return;
	}

	/* write legacy prefixes */
	size_t op_1 = op->op_1, op_2 = op->op_2;
	if (as->cur.op_count > 0 && as->cur.op[0].legacy)
	{
		asm_emit(as, as->cur.op[0].legacy);
		if (op_size(op->op_1) >= 4)
			op->op_1 >>= 1;
		if (op_size(op->op_2) >= 4)
			op->op_2 >>= 1;
	}
	
	/* swap RM to MR, makes the code following a lot easier */
	if (e == RM)
	{
		dec_t tmp1 = as->cur.op[0];
		as->cur.op[0] = as->cur.op[1];
		as->cur.op[1] = tmp1;
		size_t tmp2 = op->op_1;
		op->op_1 = op->op_2;
		op->op_2 = tmp2;
		e = MR;
	}

	dec_t *o1 = as->cur.op_count > 0 ? &as->cur.op[0] : 0;
	dec_t *o2 = as->cur.op_count > 1 ? &as->cur.op[1] : 0;
	reg_t *r1 = IS_REG(op->op_1) ? asm_decode_reg(as, 0, 0) : 0;
	reg_t *r2 = IS_REG(op->op_2) ? asm_decode_reg(as, 1, 0) : 0;
	char reg1 = r1 ? r1->val | (r1->upper << 2) : 0;
	char reg2 = r2 ? r2->val | (r2->upper << 2) : 0;
	char primary = op->primary;

	if (IS_REG(op->op_1) || IS_REG(op->op_2))
	{
		if (e == O || e == OI)
			primary += reg1;
		
		/* write REX prefix */
		if (!op->rex_long && (op->op_1 & REG64 || op->op_2 & REG64 ||
					(o1 && o1->extended) || (o2 && o2->extended) ||
					(op->op_1 & REG8 && r1->val & 0b100) ||
					(op->op_2 & REG8 && r2->val & 0b100)))
		{
			char rex = 0b01000000;

			if (op->op_1 & REG64 || op->op_2 & REG64)
				rex |= 0b1 << 3;
		
			/*
			 * the intel documentation is wrong, these two bits are required
			 * even if no ModR/M is used. thanks for that.
			 */
			if (o1 && o1->extended)
				rex |= 0b1;
			if (o2 && o2->extended)
				rex |= 0b1 << 2;
			asm_emit(as, rex);
		}
	}

	asm_emit(as, primary);
	
	if (op->secondary)
		asm_emit(as, op->secondary);

	/* write ModR/M */
	if (e == M || e == MI || e == MR)
	{
		char rm = 0b00000000;
		
		if (IS_REG(op->op_1))
			rm |= reg1;
		
		if (IS_REG(op->op_2))
			rm |= reg2 << 3;
	
		if (!(o1 && o1->rel) && !(o2 && o2->rel))
		{
			if (o1 && o1->disp != ~0)
			{
				if (imm_size(o1->disp) & IMM8)
					rm |= 0b01 << 6;
				else
					rm |= 0b10 << 6;
			}
			else if (o2 && o2->disp != ~0)
			{
				if (imm_size(o2->disp) & IMM8)
					rm |= 0b01 << 6;
				else
					rm |= 0b10 << 6;
			}
			else
				rm |= 0b11 << 6;
		}
		else
			rm |= 0b101;

		rm |= op->extension << 3;
		asm_emit(as, rm);
		
		if (o1 && o2 && o1->disp != ~0 && o2->disp != ~0)
		{
			printf("Two displacements are impossible to occur.\n");
			exit(1);
		}
		else if (o1 && o1->disp != ~0 && !o1->sym)
		{
			/* TODO: add dynamic encoding of SIB byte. */
			if (r1 && r1->val == RSP /* || R12 */)
				asm_emit(as, 0b00100100);

			if (imm_size(o1->disp) & IMM8)
				asm_emit_imm(as, IMM8, o1->disp);
			else
				asm_emit_imm(as, IMM32, o1->disp);
		}
		else if (o2 && o2->disp != ~0 && !o1->sym)
		{
			if (imm_size(o2->disp) & IMM8)
				asm_emit_imm(as, IMM8, o2->disp);
			else
				asm_emit_imm(as, IMM32, o2->disp);
		}
	}
	
	long imm;
	if (IS_IMM(op->op_1))
	{
		imm = asm_decode_imm(as, 0, 0);

		if (e == D)
		{
			imm -= as->out_count + op_size(op->op_1) + op_size(op->op_2);
			o1->rel = 1;
		}

		if (o1->sym && (o1->sym->type == EXTERN || e != D))
		{
			as->rel = realloc(as->rel, ++as->rel_count * sizeof(reloc_t));
			as->rel[as->rel_count - 1] = (reloc_t)
			{
				.type = o1->rel ? RELATIVE : ABSOLUTE,
				.sym = o1->sym - &as->sym[0],
				.addr = as->out_count - as->section_start,
				.add = o1->disp != ~0 ? o1->disp : 0
			};
			imm = 0;
		}

		if (!o1->sym && o1->def_rel)
		{
			as->def_rel = realloc(as->def_rel, ++as->def_rel_count
					* sizeof(def_reloc_t));
			as->def_rel[as->def_rel_count - 1] = (def_reloc_t)
			{
				.type = o1->rel ? RELATIVE : ABSOLUTE,
				.name = o1->op,
				.addr = as->out_count - as->section_start,
				.add = o1->disp != ~0 ? o1->disp : 0
			};
			imm = 0;
		}

		asm_emit_imm(as, op->op_1, imm);
	}

	if (IS_IMM(op->op_2))
	{
		imm = asm_decode_imm(as, 1, 0);
		
		if (o2->sym && (o2->sym->type == EXTERN || e != D))
		{
			as->rel = realloc(as->rel, ++as->rel_count * sizeof(reloc_t));
			as->rel[as->rel_count - 1] = (reloc_t)
			{
				.type = o2->rel ? RELATIVE : ABSOLUTE,
				.sym = o2->sym - &as->sym[0],
				.addr = as->out_count - as->section_start,
				.add = o2->disp != ~0 ? o2->disp : 0
			};
			imm = 0;
		}
		
		if (!o2->sym && o2->def_rel)
		{
			as->def_rel = realloc(as->def_rel, ++as->def_rel_count
					* sizeof(def_reloc_t));
			as->def_rel[as->def_rel_count - 1] = (def_reloc_t)
			{
				.type = o2->rel ? RELATIVE : ABSOLUTE,
				.name = o2->op,
				.addr = as->out_count - as->section_start,
				.add = o2->disp != ~0 ? o2->disp : 0
			};
			imm = 0;
		}
		
		asm_emit_imm(as, op->op_2, imm);
	}

	if (op->op == RM)
	{
		dec_t tmp1 = as->cur.op[0];
		as->cur.op[0] = as->cur.op[1];
		as->cur.op[1] = tmp1;
		size_t tmp2 = op->op_1;
		op->op_1 = op->op_2;
		op->op_2 = tmp2;
		e = MR;
	}
	
	op->op_1 = op_1;
	op->op_2 = op_2;
	memset(&as->cur, 0, sizeof(instr_t));
}

char asm_consume_label(asm_t *as)
{
	size_t len = strlen(as->token);
	enum symbol_type t = LABEL;

	if (len < 2 || as->token[len - 1] != ':')
		return 0;
	
	if (len > 2 && as->token[len - 2] == ':')
	{
		len--;
		t = GLOBAL_LABEL;
	}

	as->sym = realloc(as->sym, ++as->sym_count * sizeof(symbol_t));
	symbol_t *sym = &as->sym[as->sym_count - 1];
	sym->name = calloc(len + 1, 1);
	strcpy(sym->name, as->token);
	sym->name[len - 1] = '\0';
	sym->addr = as->out_count - as->section_start;
	sym->type = t;
	sym->section = ~0;
	return 1;
}

char asm_consume_extern(asm_t *as)
{
	if (as->ext == 1)
	{
		as->sym = realloc(as->sym, ++as->sym_count * sizeof(symbol_t));

		symbol_t *sym = &as->sym[as->sym_count - 1];
		size_t len = strlen(as->token);
		sym->type = EXTERN;
		sym->name = calloc(len + 1, 1);
		strcpy(sym->name, as->token);
		sym->addr = 0;
		sym->section = ~0;
		
		as->ext = 0;
		return 1;
	}

	if (strcmp(as->token, "extern") == 0)
	{
		as->ext = 1;
		return 1;
	}

	return 0;
}

void asm_close_section(asm_t *as)
{
	if (!as->section)
		return;

	for (size_t i = 0; i < as->sec_count; i++)
		if (strcasecmp(as->sec[i].name, as->section) == 0)
		{
			printf("Tried to close section %s which was already closed.\n", as->section);
			exit(1);
		}
	
	as->sec = realloc(as->sec, ++as->sec_count * sizeof(section_t));
	as->sec[as->sec_count - 1] = (section_t)
	{
		.name = as->section,
		.addr = as->section_start,
		.size = as->out_count - as->section_start
	};

	for (size_t i = 0; i < as->sym_count; i++)
		if (as->sym[i].section == ~0)
			as->sym[i].section = as->sec_count - 1;

	for (size_t i = 0; i < as->def_rel_count; i++)
	{
		def_reloc_t *rel = &as->def_rel[i];
		symbol_t *sym = asm_find_symbol(as, rel->name);

		if (!sym)
		{
			printf("Failed to lookup symbol in deferred relocation.\n");
			exit(1);
		}

		as->rel = realloc(as->rel, ++as->rel_count * sizeof(reloc_t));
		as->rel[as->rel_count - 1] = (reloc_t)
		{
			.type = rel->type,
			.sym = sym - &as->sym[0],
			.addr = rel->addr,
			.add = rel->add
		};
	}
	
	as->section = 0;
	as->section_start = 0;
}

static size_t unescape(char *s)
{
	static const char escapes[256] =
	{
		['"'] = '"',
		['\''] = '\'',
		['\\'] = '\\',
		['a'] = '\a',
		['b'] = '\b',
		['E'] = 033,
		['e'] = 033,
		['f'] = '\f',
		['n'] = '\n',
		['r'] = '\r',
		['t'] = '\t',
		['v'] = '\v'
	};
	size_t m, q;
	char *r, *w;

	for (r = w = s; *r;)
	{
		if (*r != '\\')
		{
			*w++ = *r++;
			continue;
		}
		r++;
		if (!*r)
		{
			printf("null escape sequence\n");
			exit(1);
		}
		else if (escapes[(unsigned char)*r])
			*w++ = escapes[(unsigned char)*r++];
		else if (is_odigit(*r))
		{
			for (q = 0, m = 4; m && is_odigit(*r); m--, r++)
				q = q * 8 + (*r - '0');
			*w++ = q <= 255 ? q : 255;
		}
		else if (*r == 'x' && isxdigit(r[1]))
		{
			r++;
			for (q = 0, m = 2; m && isxdigit(*r); m--, r++)
				if (isdigit(*r))
					q = q * 16 + (*r - '0');
				else
					q = q * 16 + (tolower(*r) - 'a' + 10);
			*w++ = q;
		}
		else
		{
			printf("invalid escape sequence '\\%c'\n", *r);
			exit(1);
		}
	}

	*w = '\0';
	return w - s;
}

op_t* asm_match_op(asm_t *as)
{
	/* special case for string literals */
	dec_t *dec;
	char *dup;
	size_t sub_count = 0, dup_len;

	for (size_t i = 0; i < as->cur.op_count; i++)
	{
		dec = &as->cur.op[i];
		dup = strdup(dec->op);
		dup_len = unescape(dup);

		if (dup[0] == '\"')
		{
			for (size_t j = 1; j < dup_len - 1; j++)
			{
				dec->sub = realloc(dec->sub, ++dec->sub_count * sizeof(char*));
				dec->sub[dec->sub_count - 1] = calloc(5, 1);
				sprintf(dec->sub[dec->sub_count - 1], "0x%.2x", dup[j]);
			}

			dec->sub = realloc(dec->sub, ++dec->sub_count * sizeof(char*));
			dec->sub[dec->sub_count - 1] = calloc(2, 1);
			dec->sub[dec->sub_count - 1][0] = '0';
		}
		else if (dup[0] == '_' && dup[1] == '\"')
		{
			for (size_t j = 2; j < dup_len - 1; j++)
			{
				dec->sub = realloc(dec->sub, ++dec->sub_count * sizeof(char*));
				dec->sub[dec->sub_count - 1] = calloc(5, 1);
				sprintf(dec->sub[dec->sub_count - 1], "0x%.2x", dup[j]);
			}
		}
		else
		{
			dec->sub = realloc(dec->sub, ++dec->sub_count * sizeof(char*));
			dec->sub[dec->sub_count - 1] = dup;
		}
		
		sub_count += dec->sub_count;
	}
	
	char *ops = alloca(sub_count * sizeof(size_t)), fail = 0;
	op_t *cur;

	for (size_t i = 0; i < as->cur.op_count; i++)
		for (size_t j = 0; j < as->cur.op[i].sub_count; j++)
			ops[i] = asm_resolve_op(as, i, j);

	char override_operand = 0;
scan_again:
	for (size_t i = 0; i < sizeof(op) / sizeof(op_t); i++)
	{
		cur = &op[i];

		if (strcasecmp(cur->mnemonic, as->cur.mnemonic))
			continue;

		/* special case for pseudo-ops */
		if (cur->primary == EMPTY)
		{
			for (size_t j = 0; j < sub_count; j++)
				if (!IS_IMM(ops[0]))
				{
					fail = 1;
					break;
				}

			if (!fail)
				return cur;
		}
		
		if (as->cur.op_count > 0)
		{
			/* types matching? */
			if ((IS_REG(cur->op_1) && cur->op_1 != ops[0])
				|| (IS_IMM(cur->op_1) != IS_IMM(ops[0]))
				|| (IS_IMM(cur->op_1) && cur->op_1 < ops[0]))
				continue;

			/* r/x matching? */
			if (cur->op == MR && as->cur.op[1].disp != ~0)
				continue;
		}

		if (as->cur.op_count > 1)
		{
			/* types matching? */
			if ((IS_REG(cur->op_2) && cur->op_2 != ops[1])
				|| (IS_IMM(cur->op_2) != IS_IMM(ops[1]))
				|| (IS_IMM(cur->op_2) && cur->op_2 < ops[1]))
				continue;
			
			/* r/x matching? */
			if (cur->op == RM && as->cur.op[0].disp != ~0)
				continue;
		}
		
		return cur;
	}

	if (override_operand)
	{
		/* 
		 * TODO: if we actually implement address-prefixes at some point,
		 * we have to restore the old op sizes here.
		 */
		as->cur.op[0].legacy = 0;
	}
	else if (sub_count > 0)
	{
		if (op_size(ops[0]) <= 2)
		{
			ops[0] <<= 1;
			override_operand = 1;
		}

		if (sub_count > 1 && op_size(ops[0]) <= 2)
		{
			ops[1] <<= 1;
			override_operand = 1;
		}
		
		if (override_operand)
		{
			as->cur.op[0].legacy = 0x66;
			goto scan_again;
		}
	}
	
	return 0;
}

size_t asm_resolve_op(asm_t *as, size_t i, size_t j)
{
	if (i >= as->cur.op_count)
	{
		printf("Attempted to resolve non-existing operand at index %d.\n", i);
		exit(1);
	}

	dec_t *dec = &as->cur.op[i];

	if (j >= dec->sub_count)
	{
		printf("Attempted to resolve non-existing sub-operand of %d at index %d.\n", i, j);
		exit(1);
	}

	char *op = dec->sub[j], *sign, ref = 0;
	size_t len = strlen(op);

	if (len > 2 && op[0] == '[' && op[len - 1] == ']')
	{
		op[len - 1] = '\0';
		op++;
		ref = 1;
		dec->disp = 0;

		if (sign = strchr(op, '+'))
		{
			dec->sub[j] = sign + 1;
			dec->disp = asm_decode_imm(as, i, j);
			dec->sub[j] = op;
			*sign = '\0';
		}
		else if (sign = strchr(op, '-'))
		{
			dec->sub[j] = sign + 1;
			dec->disp = -asm_decode_imm(as, i, j);
			dec->sub[j] = op;
			*sign = '\0';
		}
	}

	symbol_t *sym = asm_find_symbol(as, op);
	if (sym)
	{
		size_t addr = sym->addr;
		dec->sym = sym;
		if (ref)
			dec->rel = 1;
		op = calloc(19, 1);
		sprintf(op, "0x%.16x", addr);
	}

	dec->sub[j] = op;
	
	for (size_t i = 0; i < sizeof(reg) / sizeof(reg_t); i++)
		if (!strcasecmp(reg[i].mnemonic, op))
		{
			if (reg[i].extended)
				dec->extended = 1;
			return reg[i].size;
		}

	/* skip over the hexadecimal prefix. */
	char hex = op[0] == '0' && op[1] == 'x';
	if (hex) op += 2;
	long res = asm_decode_imm(as, i, j);
	if (!res && !sym && op[0] != '0')
		dec->rel = dec->def_rel = 1;
	return imm_size(res);
}

reg_t* asm_decode_reg(asm_t *as, size_t i, size_t j)
{
	if (i >= as->cur.op_count)
	{
		printf("Attempted to decode non-existing register at index %d.\n", i);
		exit(1);
	}

	dec_t *dec = &as->cur.op[i];

	if (j >= dec->sub_count)
	{
		printf("Attempted to decode non-existing sub-register of %d at index %d.\n", i, j);
		exit(1);
	}

	for (size_t t = 0; t < sizeof(reg) / sizeof(reg_t); t++)
		if (!strcasecmp(reg[t].mnemonic, dec->sub[j]))
			return &reg[t];

	printf("Unknown register `%s` to decode.", dec->sub[j]);
	exit(1);
	return 0;
}

long asm_decode_imm(asm_t *as, size_t i, size_t j)
{
	if (i >= as->cur.op_count)
	{
		printf("Attempted to decode non-existing immediate at index %d.\n", i);
		exit(1);
	}

	dec_t *dec = &as->cur.op[i];

	if (j >= dec->sub_count)
	{
		printf("Attempted to decode non-existing sub-immediate of %d at index %d.\n", i, j);
		exit(1);
	}

	char *op = dec->sub[j];
	long res;
	errno = 0;
	
	if (op[0] == '0' && op[1] == 'x')
		res = strtoumax(op + 2, 0, 16);
	else
		res = strtoumax(op, 0, 10);
	
	if (res == UINTMAX_MAX && errno == ERANGE)
	{
		printf("Attempted to decode huge immediate value at index %d, %d.\n", i, j);
		exit(1);
	}

	return res;
}

void asm_emit(asm_t *as, char byte)
{
	as->out = realloc(as->out, ++as->out_count);
	as->out[as->out_count - 1] = byte;
}

void asm_emit_imm(asm_t *as, size_t op, size_t val)
{
	if (op & IMM8)
		asm_emit(as, val & 0xFF);
	else if (op & IMM16)
	{
		asm_emit(as, val & 0xFF);
		asm_emit(as, (val >> 8) & 0xFF);
	}
	else if (op & IMM32)
	{
		asm_emit(as, val & 0xFF);
		asm_emit(as, (val >> 8) & 0xFF);
		asm_emit(as, (val >> 16) & 0xFF);
		asm_emit(as, (val >> 24) & 0xFF);
	}
	else if (op & IMM64)
	{
		asm_emit(as, val & 0xFF);
		asm_emit(as, (val >> 8) & 0xFF);
		asm_emit(as, (val >> 16) & 0xFF);
		asm_emit(as, (val >> 24) & 0xFF);
		asm_emit(as, (val >> 32) & 0xFF);
		asm_emit(as, (val >> 40) & 0xFF);
		asm_emit(as, (val >> 48) & 0xFF);
		asm_emit(as, (val >> 56) & 0xFF);
	}
}

void asm_emit_current_labels(asm_t *as, char *line)
{
	char *org = strdup(line), *buf;
	size_t len;
	FILE *str = open_memstream(&buf, &len);

	for (size_t i = as->last_sym_count; i < as->sym_count; i++)
		switch(as->sym[i].type)
		{
		case GLOBAL_LABEL:
			fprintf(str, "%s:: ", as->sym[i].name);
			break;
		case LABEL:
			fprintf(str, "%s: ", as->sym[i].name);
			break;
		case EXTERN:
			fprintf(str, "%s ", as->sym[i].name);
			break;
		default:
			break;
		}

	fclose(str);
	as->last_sym_count = as->sym_count;
	sprintf(line, "%-22s%s", buf, org);
}

void asm_emit_current_hex(asm_t *as, char *line)
{
	char *org = strdup(line), *buf;
	size_t len;
	FILE *str = open_memstream(&buf, &len);

	for (size_t i = as->last_out_count; i < as->out_count; i++)
	{
		if (i - as->last_out_count > 9)
		{
			fprintf(str, "...");
			break;
		}

		fprintf(str, "%02hhX ", as->out[i]);
	}

	fclose(str);
	as->last_out_count = as->out_count;
	sprintf(line, "%-38s%-20s", buf, org);
}

symbol_t* asm_find_symbol(asm_t *as, const char *name)
{
	for (size_t i = 0; i < as->sym_count; i++)
		if (strcmp(as->sym[i].name, name) == 0)
			return &as->sym[i];

	return 0;
}

symbol_t* asm_iterate_symbols(asm_t *as, size_t ind)
{
	if (ind >= as->sym_count)
		return 0;

	return &as->sym[ind];
}

reloc_t* asm_iterate_relocs(asm_t *as, size_t ind)
{
	if (ind >= as->rel_count)
		return 0;

	return &as->rel[ind];
}

section_t* asm_find_section(asm_t *as, const char *name)
{
	for (size_t i = 0; i < as->sec_count; i++)
		if (strcmp(as->sec[i].name, name) == 0)
			return &as->sec[i];

	return 0;
}

char op_size(size_t op)
{
	if (op & IMM64 || op & REG64)
		return 8;
	else if (op & IMM32 || op & REG32)
		return 4;
	else if (op & IMM16 || op & REG16)
		return 2;
	else if (op & IMM8 || op & REG8)
		return 1;

	return 0;
}

size_t imm_size(long long op)
{
	if (op <= 0xFF)
		return IMM8;
	else if (op <= 0xFFFF)
		return IMM16;
	else if (op <= 0xFFFFFFFF)
		return IMM32;
	else if (op <= ~0ULL)
		return IMM64;

	return 0;
}

