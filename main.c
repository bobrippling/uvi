#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>

#include "term.h"
#include "ncurses.h"

#include "main.h"
#include "config.h"

static void usage(const char *);

jmp_buf allocerr;


void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [-t] [--] [filename]\n", s);
	fputs("  -t: terminal mode\n", stderr);
	exit(1);
}

void bail(int sig)
{
	/* TODO: save open buffers */
	char m[] = "Received fatal signal ";

	write(STDERR_FILENO, m, sizeof(m));

	while(sig){
		*m = '0' + sig % 10;
		sig /= 10;
		write(STDERR_FILENO, m, 1);
	}

	*m = '\n';
	write(STDERR_FILENO, m, 1);

	exit(sig);
}

int main(int argc, const char **argv)
{
	int i, argv_options = 1, readonly = 0;
	int (*main2)(const char *, char) = &ncurses_main;
	const char *fname = NULL;

	for(i = 1; i < argc; i++)
		if(argv_options && *argv[i] == '-'){
			if(strlen(argv[i]) == 2){
				switch(argv[i][1]){
					case '-':
						argv_options = 0;
						break;

					case 't':
						main2 = &term_main;
						break;

					case 'R':
						readonly = 1;
						break;

					default:
						fprintf(stderr, "unknown option: \"%s\"\n", argv[i]);
						usage(*argv);
				}
			}else if(!strcmp(argv[i], "--help"))
				usage(*argv);
			else{
				fprintf(stderr, "invalid option: \"%s\"\n", argv[i]);
				usage(*argv);
			}
		}else if(!fname)
			fname = argv[i];
		else
			usage(*argv);

	if(setjmp(allocerr)){
		fprintf(stderr, PROG_NAME" panic! longjmp bail: malloc(): %s\n",
				errno ?  strerror(errno) : "(no error code)");
		return 1;
	}

	return main2(fname, readonly);
}
