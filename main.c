#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>

#include "term.h"
#include "view/ncurses.h"

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "main.h"
#include "config.h"
#include "util/alloc.h"

static void usage(const char *);

jmp_buf allocerr;
struct settings global_settings;
buffer_t *lastbuffer = NULL;
char debug = 0;

void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [-tdR"
#if VIEW_COLOUR
			"c"
#endif
			"] [--] [filename]\n", s);
	fputs("  -t: terminal mode\n", stderr);
	fputs("  -d: debug mode (don't catch SEGV)\n", stderr);
	fputs("  -R: open as read only\n", stderr);
#if VIEW_COLOUR
	fputs("  -c: colour (ncurses)\n", stderr);
#endif
	exit(1);
}

void bail(const int sig)
{
	const char m[] = "We get signal: ";
	static int saving = 0;

	if(!saving){
		/*
		 * rudimentary mutex
		 * if we're signaled again
		 * (during bail()), then something's
		 * really wrong
		 */
		saving = 1;
		if(lastbuffer)
			command_dumpbuffer(lastbuffer);
		saving = 0;
	}

	write(STDERR_FILENO, m, strlen(m));

	if(sig == SIGTERM)
		write(STDERR_FILENO, "TERM\n", 5);
	else if(sig == SIGINT)
		write(STDERR_FILENO, "INT\n", 4);
	else if(sig == SIGSEGV)
		write(STDERR_FILENO, "SEGV\n", 5);
	else
		write(STDERR_FILENO, "..?\n", 1);

	exit(sig);
}

int main(int argc, const char **argv)
{
	int i, argv_options = 1, readonly = 0;
	int (*main2)(const char *, char) = &ncurses_main;
	const char *fname = NULL;
	enum allocfail af;

	if(setlocale(LC_CTYPE, "") == NULL){
		fprintf(stderr, "%s: Locale not specified :(\n", *argv);
		return 1;
	}

	global_settings.tabstop  = DEFAULT_TAB_STOP;
	global_settings.colour   = 0;
	global_settings.showtabs = 0;

	signal(SIGHUP, &bail);

	if((af = setjmp(allocerr))){
		const char *from = NULL;

		if(lastbuffer)
			command_dumpbuffer(lastbuffer);

		switch(af){
			case ALLOC_UMALLOC:   from = "umalloc.c"; break;
			case ALLOC_BUFFER_C:  from = "buffer.c"; break;
			case ALLOC_COMMAND_C: from = "command.c"; break;
			case ALLOC_NCURSES_1: from = "ncurses.c (1)"; break;
			case ALLOC_NCURSES_2: from = "ncurses.c (2)"; break;
			case ALLOC_NCURSES_3: from = "ncurses.c (3)"; break;
			case ALLOC_VIEW:      from = "view.c"; break;
		}

		fprintf(stderr, PROG_NAME" panic! longjmp bail from %s: malloc(): %s\n",
				from, errno ?  strerror(errno) : "(no error code)");
		return 1;
	}

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

					case 'd':
						debug = 1;
						break;

#if VIEW_COLOUR
					case 'c':
						global_settings.colour = 1;
						break;
#endif

					case 'R':
						readonly = 1;
						break;

					default:
						fprintf(stderr, "unknown option: \"%s\"\n", argv[i]);
						usage(*argv);
				}
			}else if(!strcmp(argv[i], "--help")){
				usage(*argv);
			}else{
				fprintf(stderr, "invalid option: \"%s\"\n", argv[i]);
				usage(*argv);
			}
		}else{
			argv_options = 0;
			if(!fname)
				fname = argv[i];
			else
				usage(*argv);
		}

	return main2(fname, readonly);
}
