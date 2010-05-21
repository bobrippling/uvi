#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "term.h"

#include "config.h"

static void usage(const char *);

jmp_buf allocerr;


void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [filename]\n", s);
	exit(1);
}

int main(int argc, const char **argv)
{
	int i, argv_options = 1;
	const char *fname = NULL;

	for(i = 1; i < argc; i++)
		if(argv_options && *argv[i] == '-'){
			if(strlen(argv[i]) == 2){
				switch(argv[i][1]){
					case '-':
						argv_options = 0;
						break;

					default:
						usage(*argv);
				}
			}else
				usage(*argv);
		}else if(!fname)
			fname = argv[i];
		else
			usage(*argv);

	if(setjmp(allocerr)){
		fputs(PROG_NAME": malloc(): ", stderr);
		perror(NULL);
		return 1;
	}

	return term_main(fname);
}
