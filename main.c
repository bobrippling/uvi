#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "term.h"
#include "ncurses.h"

#include "config.h"

static void usage(const char *);

jmp_buf allocerr;


void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [-t] [--] [filename]\n", s);
  fputs("  -t: terminal mode\n", stderr);
	exit(1);
}

int main(int argc, const char **argv)
{
	int i, argv_options = 1, term = 0;
	const char *fname = NULL;

	for(i = 1; i < argc; i++)
		if(argv_options && *argv[i] == '-'){
			if(strlen(argv[i]) == 2){
				switch(argv[i][1]){
					case '-':
						argv_options = 0;
						break;

          case 't':
            term = 1;
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

  if(term)
    return term_main(fname);
  else
    return ncurses_main(fname);
}
