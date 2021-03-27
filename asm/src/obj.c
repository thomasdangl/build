
#include "obj.h"
#include <stdio.h>
#include <string.h>

#define ELF_SHEADER(elf) ((Elf64_Shdr *)((size_t)elf + elf->e_shoff))
#define ELF_SECTION(elf, ind) (&ELF_SHEADER(elf)[ind])

size_t asm_to_obj(asm_t *as, char **out)
{
	/* other object file formats can be added here. */
	return asm_to_elf_obj(as, out);
}

size_t asm_to_elf_obj(asm_t *as, char **out)
{
	const char* sections[] = { "", ".strtab", ".text", ".symtab", ".rela.text" };

	size_t size = sizeof(Elf64_Ehdr);
	Elf64_Ehdr *elf = calloc(1, size);

	elf->e_ident[EI_MAG0] = 0x7F;
	elf->e_ident[EI_MAG1] = 'E';
	elf->e_ident[EI_MAG2] = 'L';
	elf->e_ident[EI_MAG3] = 'F';
	elf->e_ident[EI_CLASS] = ELFCLASS64;
	elf->e_ident[EI_DATA] = ELFDATA2LSB;
	elf->e_ident[EI_VERSION] = EV_CURRENT;
	elf->e_type = ET_REL;
	elf->e_machine = EM_X86_64;
	elf->e_version = EV_CURRENT;
	elf->e_shoff = size;
	elf->e_ehsize = size;
	elf->e_shentsize = sizeof(Elf64_Shdr);
	elf->e_shnum = sizeof(sections) / sizeof(char*);
	elf->e_shstrndx = 1;

	/* create our sections here */
	for (size_t i = 0; i < elf->e_shentsize; i++)
	{
		elf = realloc(elf, size + sizeof(Elf64_Shdr));
		memset((char*) elf + size, 0, sizeof(Elf64_Shdr));
		size += sizeof(Elf64_Shdr);
	}

	Elf64_Shdr *sec = ELF_SECTION(elf, elf->e_shstrndx);
	sec->sh_type = SHT_STRTAB;
	sec->sh_offset = size;

	for (size_t i = 0; i < elf->e_shnum; i++)
	{
		ELF_SECTION(elf, i)->sh_name = sec->sh_size;
		size_t cur = strlen(sections[i]) + 1;
		elf = realloc(elf, size + sec->sh_size + cur);
		strcpy((char*) elf + size + sec->sh_size, sections[i]);
		sec->sh_size += cur;
	}
	size += sec->sh_size;

	size_t *string_ind = alloca(as->sym_count * sizeof(size_t));
	symbol_t *sy; size_t len;
	for (size_t i = 0; i < as->sym_count; i++)
	{
		sy = asm_iterate_symbols(as, i);
		len = strlen(sy->name) + 1;
		elf = realloc(elf, size + len);
		strcpy((char*) elf + size, sy->name);
		string_ind[i] = sec->sh_size;
		sec->sh_size += len;
		size += len;
	}

	Elf64_Shdr *text = ELF_SECTION(elf, 2);
	text->sh_type = SHT_PROGBITS;
	text->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	text->sh_offset = size;
	text->sh_size = as->out_count;

	elf = realloc(elf, size + as->out_count);
	memcpy((char*) elf + size, as->out, as->out_count);
	size += as->out_count;

	Elf64_Shdr *sym = ELF_SECTION(elf, 3);
	sym->sh_type = SHT_SYMTAB;
	sym->sh_offset = size;
	sym->sh_size = (as->sym_count + 1) * sizeof(Elf64_Sym);
	sym->sh_link = elf->e_shstrndx;
	sym->sh_info = 1;
	sym->sh_entsize = sizeof(Elf64_Sym);
	
	elf = realloc(elf, size + sym->sh_size);
	memset((char*) elf + size, 0, sym->sh_size);
	size += sym->sh_size;

	Elf64_Sym *esy;
	for (size_t i = 0; i < as->sym_count; i++)
	{
		sy = asm_iterate_symbols(as, i);
		esy = (Elf64_Sym*) ((char*) elf + size - (as->sym_count - i) * sizeof(Elf64_Sym));
		
		esy->st_name = string_ind[i];
		esy->st_value = sy->addr;
		esy->st_size = 1;
		esy->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);

		switch (sy->type)
		{
		case LABEL:
			esy->st_shndx = 2;
			break;
		case EXTERN:
			esy->st_shndx = SHN_UNDEF;
			break;
		default:
			break;
		}
	}
	
	Elf64_Shdr *rel = ELF_SECTION(elf, 4);
	rel->sh_type = SHT_RELA;
	rel->sh_offset = size;
	rel->sh_size = as->rel_count * sizeof(Elf64_Rela);
	rel->sh_link = 3;
	rel->sh_info = 2;
	rel->sh_entsize = sizeof(Elf64_Rela);
	
	elf = realloc(elf, size + rel->sh_size);
	memset((char*) elf + size, 0, rel->sh_size);
	size += rel->sh_size;

	Elf64_Rela *erel;
	reloc_t *re;
	for (size_t i = 0; i < as->rel_count; i++)
	{
		re = asm_iterate_relocs(as, i);
		erel = (Elf64_Rela*) ((char*) elf + size - (as->rel_count - i) * sizeof(Elf64_Rela));
		size_t sym_ind = re->sym - &as->sym[0];
		erel->r_offset = re->addr;
		erel->r_info = ELF64_R_INFO(sym_ind + 1, R_X86_64_PC32);
		erel->r_addend = -4;
	}

	*out = (char*) elf;
	return size;
}

