#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "main.h"
#include "range.h"
#include "buffer.h"
#include "global.h"
#include "gui/motion.h"
#include "gui/intellisense.h"
#include "gui/gui.h"
#include "rc.h"
#include "command.h"
#include "util/io.h"
#include "preserve.h"
#include "gui/map.h"
#include "util/alloc.h"
#include "util/str.h"
#include "util/list.h"
#include "buffers.h"

static void usage(const char *);


void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [-R] [-c] [--] [filename]\n", s);
	exit(1);
}

void die(const char *s)
{
	gui_term();
	preserve(current_buffer);
	fprintf(stderr, "dying: %s\n", s);
	exit(1);
}

void sigh(const int sig)
{
	gui_term();
	preserve(current_buffer);
	fprintf(stderr, "We get signal %d\n", sig);
	exit(sig + 128);
}

int main(int argc, const char **argv)
{
	struct list *cmds = list_new(NULL);
	int i, argv_options = 1;
	int argv_fname_start = argc;
	int ro = 0;

	if(setlocale(LC_ALL, "") == NULL)
		fprintf(stderr, "%s: Warning: Locale not specified :(\n", *argv);

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
						ro = 1;
						break;

					default:
						fprintf(stderr, "unknown option: \"%s\"\n", argv[i]);
						usage(*argv);
				}
			}else if(!strcmp(argv[i], "-")){
				argv_options = 0;
				goto at_fnames;
			}else{
				fprintf(stderr, "invalid option: \"%s\"\n", argv[i]);
				usage(*argv);
			}
		}else if(argv_options && *argv[i] == '+'){
			if(str_numeric(argv[i] + 1)){
				/* uvi +55 tim.c */
				list_append(cmds, ustrprintf("%sG", argv[i] + 1));
			}else{
				char *s = ustrdup(argv[i] + 1);
				str_escape(s);
				list_append(cmds, s);
			}
		}else{
at_fnames:
			argv_fname_start = i;
			break;
		}

	if(list_count(cmds) > 0){
		struct list *l = list_gettail(cmds);
		while(l){
			gui_queue(l->data);
			l = l->prev;
		}
	}
	list_free(cmds, free);

	if(!isatty(STDIN_FILENO))
		fputs("uvi: warning: input is not a terminal\n", stderr);
	if(!isatty(STDOUT_FILENO))
		fputs("uvi: warning: output is not a terminal\n", stderr);

	map_init();
	gui_init();
	gui_term();
	rc_read();

	buffers_init(argv + argv_fname_start, ro);

	gui_reload();
	gui_run();

	gui_term();
	map_term();

	return 0;
}
