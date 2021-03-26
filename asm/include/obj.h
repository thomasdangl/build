
#ifndef ASM_OBJ_H
#define ASM_OBJ_H

#include "asm.h"
#include <elf.h>

size_t asm_to_obj(asm_t *as, char **out);

size_t asm_to_elf_obj(asm_t *as, char **out);

#endif /* ASM_OBJ_H */

