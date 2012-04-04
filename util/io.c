#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <termios.h>
#include <ncurses.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "alloc.h"
#include "../range.h"
#include "../buffer.h"
#include "io.h"
#include "../main.h"
#include "../gui/motion.h"
#include "../gui/intellisense.h"
#include "../gui/gui.h"
#include "../util/list.h"

void input_reopen()
{
	/*if(!freopen("/dev/tty", "r", stdin) || read(0, NULL, 0) != 0)*/
	fclose(stdin);

	stdin = fopen("/dev/tty", "r");

	if(!stdin){
		gui_term();
		fprintf(stderr, "uvi: freopen(\"/dev/tty\", \"r\", stdin) = %s\n", strerror(errno));
		exit(1);
	}
}

void chomp_line()
{
	int c;
	struct termios attr;

	tcgetattr(0, &attr);

	attr.c_cc[VMIN] = sizeof(char);
	attr.c_lflag   &= ~(ICANON | ECHO);

	tcsetattr(0, TCSANOW, &attr);

	c = getchar();

	attr.c_cc[VMIN] = 0;
	attr.c_lflag   |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &attr);

	ungetch(c);
}

void dumpbuffer(buffer_t *b)
{
	char dump_postfix[] = "_dump_a";
	FILE *f;
	struct stat st;
	char *path, freepath = 0, *prefixletter;

	if(!b)
		return;

	path = dump_postfix;

	if(buffer_hasfilename(b)){
		char *const s = malloc(strlen(buffer_filename(b)) + strlen(dump_postfix) + 1);
		if(s){
			freepath = 1;
			strcpy(s, buffer_filename(b));
			strcat(s, dump_postfix);
			path = s;
		}
	}

	prefixletter = path + strlen(path) - 1;
	/*
	 * if file exists, increase dump prefix,
	 * else (and on stat error), save
	 */
	do
		if(stat(path, &st) == 0)
			/* exists */
			++*prefixletter;
		else
			break;
	while(*prefixletter < 'z');
	/* if it's 'z', the user should clean their freakin' directory */

	f = fopen(path, "w");

	if(freepath)
		free(path);

	if(f){
		buffer_dump(b, f);
		fclose(f);
	}
}

char *fline(FILE *f, int *eol)
{
	int c, pos, len;
	char *line;

	if(feof(f) || ferror(f))
		return NULL;

	pos = 0;
	len = 10;
	line = umalloc(len);

	do{
		errno = 0;
		if((c = fgetc(f)) == EOF){
			if(errno == EINTR)
				continue;
			if(pos){
				if(eol)
					*eol = 0;
				return line;
			}
			free(line);
			return NULL;
		}

		line[pos++] = c;
		if(pos == len){
			len *= 2;
			line = urealloc(line, len);
			line[pos] = '\0';
		}

		if(c == '\n'){
			line[pos-1] = '\0';
			if(eol)
				*eol = 1;
			return line;
		}
	}while(1);
}
