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

int canwrite(mode_t mode, uid_t uid, gid_t gid);


void input_reopen()
{
	freopen("/dev/tty", "r", stdin);
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

void *readfile(const char *filename)
{
	buffer_t *b = NULL;

	if(filename){
		FILE *f;

		if(strcmp(filename, "-")){
			f = fopen(filename, "r");
		}else{
			f = stdin;
		}

		if(f){
			int nread = buffer_read(&b, f);

			if(nread == -1){
				gui_status(GUI_ERR, "read \"%s\": %s",
						filename,
						errno ? strerror(errno) : "unknown error - binary file?");

			}else{
				buffer_readonly(b) = access(filename, W_OK);

				if(nread == 0)
					gui_status(GUI_NONE, "(empty file)%s", buffer_readonly(b) ? " [read only]" : "");
				else
					gui_status(GUI_NONE, "%s%s: %dC, %dL%s%s",
							filename,
							buffer_readonly(b) ? " [read only]" : "",
							buffer_nchars(b),
							buffer_nlines(b),
							buffer_eol(b)  ? "" : " [noeol]",
							buffer_crlf(b) ? " [crlf]" : ""
							);
			}

			if(f == stdin)
				input_reopen();
			else
				fclose(f);

		}else{
			if(errno == ENOENT)
				goto newfile;
			gui_status(GUI_ERR, "open \"%s\": %s", filename, strerror(errno));
		}

	}else{
newfile:
		if(filename)
			gui_status(GUI_NONE, "%s: new file", filename);
		else
			gui_status(GUI_NONE, "(new file)");
	}

	if(!b)
		b = buffer_new_empty();
	if(filename && strcmp(filename, "-"))
		buffer_setfilename(b, filename);

	return b;
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
