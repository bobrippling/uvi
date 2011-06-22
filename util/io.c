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

int canwrite(mode_t mode, uid_t uid, gid_t gid)
{
	if(mode & 02)
		return 1;

	/* can't be bothered checking all groups */
	if((mode & 020) && gid == getgid())
		return 1;

	if((mode & 0200) && uid == getuid())
		return 1;

	return 0;
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
				gui_status(GUI_ERR, "\"%s\" [%s]",
						filename,
						errno ? strerror(errno) : "unknown error - binary file?");

			}else{
				/* end up here on successful read */
				struct stat st;

				if(!stat(filename, &st))
					buffer_readonly(b) = !canwrite(st.st_mode, st.st_uid, st.st_gid);
				else
					buffer_readonly(b) = 0;

				if(nread == 0)
					gui_status(GUI_NONE, "(empty file)%s", buffer_readonly(b) ? " [read only]" : "");
				else
					gui_status(GUI_NONE, "%s%s: %dC, %dL%s", filename,
							buffer_readonly(b) ? " [read only]" : "",
							buffer_nchars(b), buffer_nlines(b),
							buffer_eol(b) ? "" : " [noeol]");
			}

			if(f == stdin)
				input_reopen();
			else
				fclose(f);

		}else{
			if(errno == ENOENT)
				goto newfile;
			gui_status(GUI_ERR, "%s: %s", filename, strerror(errno));
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
