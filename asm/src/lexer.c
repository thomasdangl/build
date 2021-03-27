
#include "lexer.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

lexer_t* lexer_init(const char *file)
{
	lexer_t *lex =  calloc(1, sizeof(lexer_t));
	FILE *fp = fopen(file, "r");

	if (!fp)
	{
		printf("Failed to open input file `%s`.\n", file);
		exit(1);
	}

	/* determine eof offset. */
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* read in the file. */
	char *fi = calloc(size + 1, sizeof(char));
	fread(fi, 1, size, fp);
	fclose(fp);

	/* create our loc mapping. */
	char *it = strchr(fi, '\n'), *pit = fi;
	while (it != 0)
	{
		lex->loc = realloc(lex->loc, ++lex->loc_count * sizeof(loc_t));
		lex->loc[lex->loc_count - 1] = (loc_t) { pit, 0, 0 };
		*it = '\0';
		pit = it + 1;
		it = strchr(pit, '\n');
	}

	lex->cur = lex->loc;
	return lex;
}

char* lexer_advance(lexer_t *lex)
{
	char *next = lex->cur->next, *end;
	
	if (!lex->cur->dup)
	{
		lex->cur->dup = strdup(lex->cur->str);
		char *end = strchr(lex->cur->dup, '#');
		if (end != 0) *end = '\0';
		lex->cur->p = strtok(lex->cur->dup, " ");
		lex->cur->next = 0;
	}
	else if (next)
	{
		lex->cur->next = 0;
		return next;
	}

	// advance once more so we can peek into the next token.
	char* token = lex->cur->p;
	if (token)
	{
		/* special case of string literals that require look-ahead */
		if (strcmp(token, "db") == 0)
		{
			size_t len = strlen(token);
			next = token + len + 1;

			if (strlen(next) > 1 && (next[0] == '\"'
						|| next[0] == '_' && next[1] == '\"'))
			{
				end = strchr(next + 2, '\"');
				
				if (end)
				{
					lex->cur->next = strdup(next);
					*(lex->cur->next + (end - next) + 1) = '\0';
					lex->cur->p = strtok(end + 1, ", ");
					return token;
				}
			}
		}
		
		lex->cur->p = strtok(0, ", ");
		return token;
	}

	return ++lex->cur <= &lex->loc[lex->loc_count - 1] ?
		lexer_advance(lex) : 0;
}

char* lexer_peek(lexer_t *lex)
{
	if (lex->cur->next)
		return lex->cur->next;

	return lex->cur->p;
}

size_t lexer_loc(lexer_t *lex)
{
	return lex->cur - &lex->loc[0];
}

