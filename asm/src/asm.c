
#include "asm.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#define is_odigit(c)  ('0' <= c && c <= '7')

op_t op[] =
{
	/* mn | Op/En | REX long | OP1 | OP2 | CODE_PRIMARY | CODE_SECONDARY | CODE_EXTENSION */
	{ "lea", RM, FALSE, REG64, IMM32, 0x8D, EMPTY, EMPTY },
	{ "mov", OI, FALSE, REG32, IMM32, 0xB8, EMPTY, EMPTY },
	{ "mov", OI, FALSE, REG64, IMM64, 0xB8, EMPTY, EMPTY },
	{ "mov", MR, FALSE, REG64, REG64, 0x89, EMPTY, EMPTY },
	{ "mov", RM, FALSE, REG64, REG64, 0x8B, EMPTY, EMPTY },
	{ "push", O, TRUE, REG64, REG64, 0x50, EMPTY, EMPTY },
	{ "pop", O, TRUE, REG64, REG64, 0x58, EMPTY, EMPTY },
	{ "add", MI, FALSE, REG64, IMM32, 0x81, EMPTY, EMPTY },
	{ "add", MR, FALSE, REG64, REG64, 0x01, EMPTY, EMPTY },
	{ "add", RM, FALSE, REG64, REG64, 0x03, EMPTY, EMPTY },
	{ "inc", M, FALSE, REG64, EMPTY, 0xFF, EMPTY, EMPTY },
	{ "sub", MI, FALSE, REG64, IMM32, 0x81, EMPTY, 0x05 },
	{ "sub", MR, FALSE, REG64, REG32, 0x29, EMPTY, EMPTY },
	{ "sub", RM, FALSE, REG64, REG32, 0x2B, EMPTY, EMPTY },
	{ "dec", M, FALSE, REG64, EMPTY, 0xFF, EMPTY, 0x01 },
	{ "xor", MR, FALSE, REG64, REG64, 0x31, EMPTY, EMPTY },
	{ "cmp", MI, FALSE, REG64, IMM32, 0x81, EMPTY, 0x07 },
	{ "test", MI, FALSE, REG64, IMM32, 0xF7, EMPTY, EMPTY },
	{ "je", D, FALSE, IMM8, EMPTY, 0x74, EMPTY, EMPTY },
	{ "jne", D, FALSE, IMM8, EMPTY, 0x75, EMPTY, EMPTY },
	{ "int", I, FALSE, IMM8, EMPTY, 0xCD, 0x00, EMPTY },
	{ "syscall", ZO, FALSE, EMPTY, EMPTY, 0x0F, 0x05, EMPTY },
	{ "call", D, FALSE, IMM32, EMPTY, 0xE8, EMPTY, EMPTY },
	{ "ret", ZO, FALSE, EMPTY, EMPTY, 0xC3, EMPTY, EMPTY },
	{ "db", EMPTY, FALSE, IMM8, EMPTY, EMPTY, EMPTY, EMPTY }
};

reg_t reg[] =
{
	{ "rax", RAX, REG64 },
	{ "eax", RAX, REG32 },
	{ "ax", RAX, REG16 },
	{ "rcx", RCX, REG64 },
	{ "ecx", RCX, REG32 },
	{ "cx", RCX, REG16 },
	{ "rdx", RDX, REG64 },
	{ "edx", RDX, REG32 },
	{ "cx", RDX, REG16 },
	{ "rbx", RBX, REG64 },
	{ "ebx", RBX, REG32 },
	{ "bx", RBX, REG16 },
	{ "rsp", RSP, REG64 },
	{ "esp", RSP, REG32 },
	{ "sp", RSP, REG16 },
	{ "rbp", RBP, REG64 },
	{ "ebp", RBP, REG32 },
	{ "bp", RBP, REG16 },
	{ "rsi", RSI, REG64 },
	{ "esi", RSI, REG32 },
	{ "si", RSI, REG16 },
	{ "rdi", RDI, REG64 },
	{ "edi", RDI, REG32 },
	{ "di", RDI, REG16 }
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
			{ .op = as->token, .sym = 0, .disp = ~0, .rel = 0  };
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
			printf("OP %d: %s\n", i, as->cur.op[i]);
		exit(1);
	}

	/* handle pseudo-instructions first */
	if (op->primary == EMPTY)
	{
		for (size_t i = 0; i < as->cur.op_count; i++)
			for (size_t j = 0; j < as->cur.op[i].sub_count; j++)
				asm_emit_imm(as, op->op_1, asm_decode_imm(as, i, j));
		memset(&as->cur, 0, sizeof(instr_t));
		return;
	}

	symbol_t *sym;
	dec_t *dec = &as->cur.op[0];
	char primary = op->primary;

	if (IS_REG(op->op_1))
	{
		if (op->op == O || op->op == OI)
			primary += asm_decode_reg(as, 0, 0);
		
		/* write REX prefix */
		if (!op->rex_long && op->op_1 & REG64)
			asm_emit(as, 0b01001000);
	}

	asm_emit(as, primary);

	/* write ModR/M */
	if (op->op == M || op->op == MI || op->op == MR || op->op == RM)
	{
		char rm = 0b00000000;
		
		if (op->op == M || op->op == MI || op->op == MR)
		{
			if (IS_REG(op->op_1))
				rm |= asm_decode_reg(as, 0, 0);
			
			if (IS_REG(op->op_2))
				rm |= asm_decode_reg(as, 1, 0) << 3;
		}
		else if (op->op == RM)
		{
			dec = &as->cur.op[1];

			if (IS_REG(op->op_1))
				rm |= asm_decode_reg(as, 0, 0) << 3;

			if (IS_REG(op->op_2))
				rm |= asm_decode_reg(as, 1, 0);
		}
	
		if (!dec->rel)
		{	
			/* TODO: use 8-bit when possible */
			if (dec->disp != ~0)
				rm |= 0b10 << 6;
			else
				rm |= 0b11 << 6;
		}
		else
			rm |= 0b101;

		rm |= op->extension << 3;
		asm_emit(as, rm);
		
		if (dec->disp != ~0)
			asm_emit_imm(as, IMM32, dec->disp);
	}
	
	if (op->secondary)
		asm_emit(as, op->secondary);
	
	long imm;
	if (IS_IMM(op->op_1))
	{
		imm = asm_decode_imm(as, 0, 0);
		dec = &as->cur.op[0];

		if (op->op == D)
		{
			imm -= as->out_count + op_size(op->op_1) + op_size(op->op_2);
			dec->rel = 1;
		}

		if (dec && dec->sym && dec->sym->type == EXTERN)
		{
			as->rel = realloc(as->rel, ++as->rel_count * sizeof(reloc_t));
			as->rel[as->rel_count - 1] = (reloc_t)
			{
				.sym = dec->sym,
				.type = dec->rel ? RELATIVE : ABSOLUTE,
				.addr = as->out_count - as->section_start
			};
			imm = 0;
		}

		asm_emit_imm(as, op->op_1, imm);
	}

	if (IS_IMM(op->op_2))
	{
		imm = asm_decode_imm(as, 1, 0);
		dec = &as->cur.op[1];
		
		if (dec && dec->sym)
		{
			as->rel = realloc(as->rel, ++as->rel_count * sizeof(reloc_t));
			as->rel[as->rel_count - 1] = (reloc_t)
			{
				.sym = dec->sym,
				.type = dec->rel ? RELATIVE : ABSOLUTE,
				.addr = as->out_count - as->section_start
			};
			imm = 0;
		}
		
		asm_emit_imm(as, op->op_2, imm);
	}

	memset(&as->cur, 0, sizeof(instr_t));
}

char asm_consume_label(asm_t *as)
{
	size_t len = strlen(as->token);

	if (as->token[len - 1] != ':')
		return 0;

	as->sym = realloc(as->sym, ++as->sym_count * sizeof(symbol_t));
	symbol_t *sym = &as->sym[as->sym_count - 1];
	sym->type = LABEL;
	sym->name = calloc(len + 1, 1);
	strcpy(sym->name, as->token);
	sym->name[len - 1] = '\0';
	sym->addr = as->out_count - as->section_start;
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
	size_t sub_count = 0;

	for (size_t i = 0; i < as->cur.op_count; i++)
	{
		dec = &as->cur.op[i];
		dup = strdup(dec->op);
		unescape(dup);

		if (dup[0] == '\"')
		{
			for (size_t j = 1; j < strlen(dup) - 1; j++)
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
			for (size_t j = 2; j < strlen(dup) - 1; j++)
			{
				dec->sub = realloc(dec->sub, ++dec->sub_count * sizeof(char*));
				dec->sub[dec->sub_count - 1] = calloc(5, 1);
				sprintf(dec->sub[dec->sub_count - 1], "0x%.2x", dup[j]);
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
		{
			asm_resolve_op(as, i, j);
			ops[i] = match_op(as->cur.op[i].sub[j]);
		}

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
				|| (IS_IMM(cur->op_1) != IS_IMM(ops[0])))
				continue;

			/* r/x matching? */
			if (cur->op == MR && as->cur.op[1].disp != ~0)
				continue;
		}

		if (as->cur.op_count > 1)
		{
			/* types matching? */
			if ((IS_REG(cur->op_2) && cur->op_2 != ops[1])
				|| IS_IMM(cur->op_2) != IS_IMM(ops[1]))
				continue;
			
			/* r/x matching? */
			if (cur->op == RM && as->cur.op[0].disp != ~0)
				continue;
		}
		
		return cur;
	}

	return 0;
}

void asm_resolve_op(asm_t *as, size_t i, size_t j)
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

	char *op = dec->sub[j], *sign;
	size_t len = strlen(op);

	if (len > 2 && op[0] == '[' && op[len - 1] == ']')
	{
		op[len - 1] = '\0';
		op++;
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

		if (dec->disp == 0)
		{
			dec->disp = ~0;
			dec->rel = 1;
		}

		op = calloc(19, 1);
		sprintf(op, "0x%.16x", addr);
	}

	dec->sub[j] = op;
}

char asm_decode_reg(asm_t *as, size_t i, size_t j)
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
			return reg[t].val;

	printf("Unknown register `%s` to decode.", dec->sub[j]);
	exit(1);
	return 0xFF;
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

char match_op(char *op)
{
	for (size_t i = 0; i < sizeof(reg) / sizeof(reg_t); i++)
		if (!strcasecmp(reg[i].mnemonic, op))
			return reg[i].size;

	/* skip over the hexadecimal prefix. */
	char hex = op[0] == '0' && op[1] == 'x';
	if (hex) op += 2;

	for (size_t i = 0; i < strlen(op); i++)
		if (!isdigit(op[i]) && !(hex && op[i] >= 'a' && op[i] <= 'f'))
		{
			printf("Operand `%s` could not be matched.\n", op);
			exit(1);
		}

	return IMM8;
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

