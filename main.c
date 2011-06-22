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

static void usage(const char *);


void usage(const char *s)
{
	fprintf(stderr, "Usage: %s [-R] [-c] [--] [filename]\n", s);
	exit(1);
}

void die(const char *s)
{
	gui_term();
	preserve(global_buffer);
	fprintf(stderr, "dying: %s\n", s);
	exit(1);
}

void sigh(const int sig)
{
	gui_term();
	preserve(global_buffer);
	fprintf(stderr, "We get signal %d\n", sig);
	exit(sig + 128);
}

int main(int argc, const char **argv)
{
	struct list *cmds = list_new(NULL);
	int i, argv_options = 1, readonly = 0;
	const char *fname = NULL;

	if(setlocale(LC_ALL, "") == NULL){
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
			}else if(!strcmp(argv[i], "-")){
				argv_options = 0;
				fname = argv[i];
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
			argv_options = 0;
			if(!fname)
				fname = argv[i];
			else
				usage(*argv);
		}

	if(list_count(cmds) > 0){
		struct list *l = list_gettail(cmds);
		while(l){
			gui_queue(l->data);
			l = l->prev;
		}
	}
	list_free(cmds, free);

	if(!isatty(0))
		fputs("uvi: warning: input is not a terminal\n", stderr);
	if(!isatty(1))
		fputs("uvi: warning: output is not a terminal\n", stderr);

	map_init();
	gui_init();
	gui_term();
	rc_read();

	gui_reload();
	global_buffer = readfile(fname);
	if(readonly)
		buffer_readonly(global_buffer) = 1;
	gui_run();

	gui_term();
	map_term();
	buffer_free(global_buffer);

	return 0;
}
