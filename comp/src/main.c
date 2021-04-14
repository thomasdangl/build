
#include "parser.h"
#include "codegen.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	if (argc != 2 && argc != 3)
	{
		printf("Invalid arguments to asm program.\n");
		exit(1);
	}

	parser_t *par = parser_init(argv[1]);
	node_t *ast = parser_full_pass(par);
	codegen_t *cg = codegen_init(ast);
	codegen_pretty_print(cg);

	if (argc == 3)
	{
		size_t size = strlen(cg->scratch);

		FILE *fp = fopen(argv[2], "wb");

		if (!fp)
		{
			printf("Failed to open output file `%s`.\n", argv[2]);
			exit(1);
		}

		fwrite(cg->scratch, size, 1, fp);
		fclose(fp);
		printf("Wrote %d bytes to `%s`.\n", size, argv[2]);
	}

	return 0;
}

