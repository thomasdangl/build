
#include "obj.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	if (argc != 2 && argc != 3)
	{
		printf("Invalid arguments to asm program.\n");
		exit(1);
	}

	lexer_t* lex = lexer_init(argv[1]);
	asm_t* as = asm_init(lex);
	asm_full_pass(as);

	if (argc == 3)
	{
		char *out;
		size_t size = asm_to_obj(as, &out);

		FILE *fp = fopen(argv[2], "wb");

		if (!fp)
		{
			printf("Failed to open output file `%s`.\n", argv[2]);
			exit(1);
		}

		fwrite(out, size, 1, fp);
		fclose(fp);
		printf("Wrote %d bytes to `%s`.\n", size, argv[2]);
	}

	return 0;
}

