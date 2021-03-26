
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
	const char* sections[] = { "", ".strtab", ".text", ".symtab" };

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

	/* TODO: add all strings for symbols here */
	const char start[] = "_start";
	elf = realloc(elf, size + sizeof(start));
	strcpy((char*) elf + size, start);
	size_t start_off = sec->sh_size;
	sec->sh_size += sizeof(start);
	size += sizeof(start);

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
	/*
	 * for some reason, libbfd cannot read the first symbol correctly.
	 * according to the elf specification, there should be no alignment constraints
	 * and readelf perfectly reads the first symbol as well.
	 * in order to keep compatibility with GNU, we keep the first symbol empty.
	 */
	sym->sh_size = 2 * sizeof(Elf64_Sym);
	sym->sh_link = elf->e_shstrndx;
	sym->sh_info = 1;
	sym->sh_entsize = sizeof(Elf64_Sym);
	
	/* TODO: add all symbols here */
	elf = realloc(elf, size + sym->sh_size);
	Elf64_Sym *start_sym = (Elf64_Sym*) ((char*) elf + size + sizeof(Elf64_Sym));
	memset((char*) elf + size, 0, sym->sh_size);
	size += sym->sh_size;

	symbol_t *start_label = asm_find_symbol(as, start);
	if (!start_label || start_label->type != LABEL)
	{
		printf("Could not find symbol `%s` (LABEL).\n", start);
		exit(1);
	}

	start_sym->st_name = start_off;
	start_sym->st_value = start_label->addr;
	start_sym->st_size = as->out_count;
	start_sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
	start_sym->st_shndx = 2;
	
	*out = (char*) elf;
	return size;
}

