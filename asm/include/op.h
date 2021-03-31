
#ifndef ASM_OP_H
#define ASM_OP_H

#define EMPTY 0
#define REG8 (1 << 0)
#define REG16 (1 << 1)
#define REG32 (1 << 2)
#define REG64 (1 << 3)
#define IMM8 (1 << 4)
#define IMM16 (1 << 5)
#define IMM32 (1 << 6)
#define IMM64 (1 << 7)

#define IS_REG(x) ((x & REG8) || (x & REG16) || (x & REG32) || (x & REG64))
#define IS_IMM(x) ((x & IMM8) || (x & IMM16) || (x & IMM32) || (x & IMM64))

#define RAX 0b000
#define RCX 0b001
#define RDX 0b010
#define RBX 0b011
#define RSP 0b100
#define RBP 0b101
#define RSI 0b110
#define RDI 0b111
#define R8 0b000
#define R9 0b001
#define R10 0b010
#define R11 0b011
#define R12 0b100
#define R13 0b101
#define R14 0b110
#define R15 0b111

#define TRUE (1)
#define FALSE (0)

enum operand_encoding_type
{
	D = 1,
	I,
	M,
	O,
	Z,
	MI,
	MR,
	OI,
	RM,
	ZO
};

typedef struct
{
	char* mnemonic;
	enum operand_encoding_type op;
	char rex_long;
	size_t op_1;
	size_t op_2;
	size_t primary;
	size_t secondary;
	size_t extension;
} op_t;

typedef struct
{
	char* mnemonic;
	size_t val;
	size_t size;
	char extended;
	char upper;
} reg_t;

char op_size(size_t op);
size_t imm_size(long long op);

extern reg_t reg[];
extern op_t op[];

#endif /* ASM_OP_H */

