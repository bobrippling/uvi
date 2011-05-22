#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>
#include <string.h>
#include <stdarg.h>

#include "main.h"
#include "range.h"
#include "buffer.h"
#include "global.h"
#include "gui/motion.h"
#include "gui/gui.h"

static void usage(const char *);


void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [-R] [-c] [--] [filename]\n", s);
	exit(1);
}

void die(const char *s)
{
	fprintf(stderr, "dying: %s\n", s);
	exit(1);
}

void sigh(const int sig)
{
	gui_term();
	fprintf(stderr, "We get signal %d\n", sig);
	exit(sig + 128);
}

int main(int argc, const char **argv)
{
	int i, argv_options = 1, readonly = 0;
	const char *fname = NULL;

	if(setlocale(LC_CTYPE, "") == NULL){
		fprintf(stderr, "%s: Locale not specified :(\n", *argv);
		return 1;
	}

	signal(SIGHUP,  &sigh);
	signal(SIGINT,  &sigh);
	signal(SIGTERM, &sigh);
	signal(SIGQUIT, &sigh);

	for(i = 1; i < argc; i++)
		if(argv_options && *argv[i] == '-'){
			if(strlen(argv[i]) == 2){
				switch(argv[i][1]){
					case '-':
						argv_options = 0;
						break;

					case 'c':
					{
						extern struct settings global_settings;
						global_settings.colour = 1;
						break;
					}

					case 'R':
						readonly = 1;
						break;

					default:
						fprintf(stderr, "unknown option: \"%s\"\n", argv[i]);
						usage(*argv);
				}
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

	return gui_main(fname, readonly);
}
