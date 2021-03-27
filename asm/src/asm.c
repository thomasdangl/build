
#include "asm.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#define is_odigit(c)  ('0' <= c && c <= '7')

op_t op[] =
{
	{ "lea", RM, REG64, IMM32, 0x8D, EMPTY },
	{ "mov", OI, REG32, IMM32, 0xB8, EMPTY },
	{ "mov", OI, REG64, IMM64, 0xB8, EMPTY },
	{ "xor", MR, REG64, REG64, 0x31, EMPTY },
	{ "int", I, IMM8, EMPTY, 0xCD, 0x00 },
	{ "syscall", ZO, EMPTY, EMPTY, 0x0F, 0x05 },
	{ "call", D, IMM32, EMPTY, 0xE8, EMPTY },
	{ "ret", ZO, EMPTY, EMPTY, 0xC3, EMPTY },
	{ "db", EMPTY, IMM8, EMPTY, EMPTY, EMPTY }
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
	as->section = ".text";
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
		as->section = 0;
		*new = 3;
		return;
	}

	if (!as->section)
	{
		as->section = as->token;
		*new = 3;
		return;
	}

	if (!as->cur.mnemonic)
		as->cur.mnemonic = as->token;
	else
	{
		as->cur.op = realloc(as->cur.op, ++as->cur.op_count * sizeof(char*));
		as->cur.op_sym = realloc(as->cur.op_sym, as->cur.op_count * sizeof(symbol_t*));
		as->cur.op[as->cur.op_count - 1] = as->token;
		as->cur.op_sym[as->cur.op_count - 1] = 0;
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
			asm_emit_imm(as, op->op_1, asm_decode_imm(as, i));
		memset(&as->cur, 0, sizeof(instr_t));
		return;
	}

	symbol_t *sym;
	char primary = op->primary;

	if (IS_REG(op->op_1))
	{
		if (op->op == OI)
			primary += asm_decode_reg(as, 0);

		/* write REX prefix */
		if (op->op_1 & REG64)
			asm_emit(as, 0b01001000);
	}

	asm_emit(as, primary);

	/* write ModR/M */
	/* TODO: these need to be dynamic at some point in the future */
	if (IS_REG(op->op_1) && op->op == RM)
	{
		/* mod = 00 (register-indirect), reg = as decoded, rm = 101 (RIP + disp32) */
		char rm = 0b00000101;
		rm |= asm_decode_reg(as, 0) << 3;
		asm_emit(as, rm);
	}
	
	if (IS_REG(op->op_2) && op->op == MR)
	{
		/* mod = 00 (register-direct), reg = as decoded, rm = 000 (r/m) */
		char rm = 0b11000000;
		rm |= asm_decode_reg(as, 1) << 3;
		asm_emit(as, rm);
	}
	
	if (op->secondary)
		asm_emit(as, op->secondary);
	
	long imm;
	if (IS_IMM(op->op_1))
	{
		imm = asm_decode_imm(as, 0);
		sym = as->cur.op_sym[0];

		/* TODO: can we include RIP relative addressing here aswell? */
		if (op->op == D)
			imm -= as->out_count + op_size(op->op_1) + op_size(op->op_2);

		if (sym->type == EXTERN)
		{
			as->rel = realloc(as->rel, ++as->rel_count * sizeof(reloc_t));
			as->rel[as->rel_count - 1].sym = sym;
			as->rel[as->rel_count - 1].addr = as->out_count;
			imm = 0;
		}

		asm_emit_imm(as, op->op_1, imm);
	}

	if (IS_IMM(op->op_2))
		asm_emit_imm(as, op->op_2, asm_decode_imm(as, 1));

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
	sym->addr = as->out_count;

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
	size_t cur_op_count = 0;
	char **cur_op = 0;
	
	/* special case for string literals */
	for (size_t i = 0; i < as->cur.op_count; i++)
	{		
		char *cur = strdup(as->cur.op[i]);
		unescape(cur);

		if (cur[0] == '\"')
		{
			for (size_t j = 1; j < strlen(cur) - 1; j++)
			{
				cur_op = realloc(cur_op, ++cur_op_count * sizeof(char*));
				cur_op[cur_op_count - 1] = calloc(5, 1);
				sprintf(cur_op[cur_op_count - 1], "0x%.2x", cur[j]);
			}

			cur_op = realloc(cur_op, ++cur_op_count * sizeof(char*));
			cur_op[cur_op_count - 1] = calloc(2, 1);
			cur_op[cur_op_count - 1][0] = '0';
		}
		else if (cur[0] == '_' && cur[1] == '\"')
		{
			for (size_t j = 2; j < strlen(cur) - 1; j++)
			{
				cur_op = realloc(cur_op, ++cur_op_count * sizeof(char*));
				cur_op[cur_op_count - 1] = calloc(5, 1);
				sprintf(cur_op[cur_op_count - 1], "0x%.2x", cur[j]);
			}
		}
		else
		{
			cur_op = realloc(cur_op, ++cur_op_count * sizeof(char*));
			cur_op[cur_op_count - 1] = cur;
		}
	}
	
	as->cur.op = cur_op;
	as->cur.op_sym_count = as->cur.op_count;
	as->cur.op_count = cur_op_count;

	char *ops = alloca(as->cur.op_count * sizeof(size_t)), fail = 0;
	op_t *cur;

	for (size_t i = 0; i < as->cur.op_count; i++)
	{
		asm_resolve_op(as, i);
		ops[i] = match_op(as->cur.op[i]);
	}

	for (size_t i = 0; i < sizeof(op) / sizeof(op_t); i++)
	{
		cur = &op[i];

		if (strcasecmp(cur->mnemonic, as->cur.mnemonic))
			continue;

		/* special case for pseudo-ops */
		if (cur->primary == EMPTY)
		{
			for (size_t j = 0; j < as->cur.op_count; j++)
				if (!IS_IMM(ops[0]))
				{
					fail = 1;
					break;
				}

			if (!fail)
				return cur;
		}
		
		if (as->cur.op_count > 0 && ((IS_REG(cur->op_1) && cur->op_1 != ops[0])
					|| (IS_IMM(cur->op_1) != IS_IMM(ops[0]))))
			continue;

		if (as->cur.op_count > 1 && ((IS_REG(cur->op_2) && cur->op_2 != ops[1])
					|| IS_IMM(cur->op_2) != IS_IMM(ops[1])))
			continue;
		
		return cur;
	}

	return 0;
}

void asm_resolve_op(asm_t *as, size_t ind)
{
	if (ind >= as->cur.op_count)
	{
		printf("Attempted to solve non-existing operand at index %d.\n", ind);
		exit(1);
	}

	char *op = as->cur.op[ind], rel = 0;
	size_t len = strlen(op);

	if (len > 2 && op[0] == '[' && op[len - 1] == ']')
	{
		op[len - 1] = '\0';
		op++;
		rel = 1;
	}

	symbol_t *sym = asm_find_symbol(as, op);
	if (sym)
	{
		size_t addr = sym->addr;
		as->cur.op_sym[ind] = sym;

		/* TODO: here we need to know the real instruction length */
		if (rel)
			addr -= as->out_count + 7;

		op = calloc(19, 1);
		sprintf(op, "0x%.16x", addr);
	}

	as->cur.op[ind] = op;
}

char asm_decode_reg(asm_t *as, size_t ind)
{
	if (ind >= as->cur.op_count)
	{
		printf("Attempted to decode non-existing register at index %d.\n", ind);
		exit(1);
	}

	for (size_t i = 0; i < sizeof(reg) / sizeof(reg_t); i++)
		if (!strcasecmp(reg[i].mnemonic, as->cur.op[ind]))
			return reg[i].val;

	printf("Unknown register `%s` to decode.", as->cur.op[ind]);
	exit(1);
	return 0xFF;
}

long asm_decode_imm(asm_t *as, size_t ind)
{
	if (ind >= as->cur.op_count)
	{
		printf("Attempted to decode non-existing immediate value at index %d.\n", ind);
		exit(1);
	}

	long dec;
	errno = 0;
	
	if (as->cur.op[ind][0] == '0' && as->cur.op[ind][1] == 'x')
		dec = strtoumax(as->cur.op[ind] + 2, 0, 16);
	else
		dec = strtoumax(as->cur.op[ind], 0, 10);
	
	if (dec == UINTMAX_MAX && errno == ERANGE)
	{
		printf("Attempted to decode huge immediate value at index %d.\n", ind);
		exit(1);
	}

	return dec;
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

