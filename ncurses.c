#include <ncurses.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>

#include "buffer.h"
#include "range.h"
#include "command.h"
#include "list.h"
#include "main.h"
#include "view.h"

#include "ncurses.h"
#include "config.h"

#define CTRL_AND(c) ((c) & 037)
#define C_ESC			 27
#define C_EOF			 4
#define C_DEL			 127
#define C_BACKSPACE 263
#define C_NEWLINE	 '\r'
#define C_CTRL_C		3

static void nc_up(void);
static void nc_down(void);
static void sigh(int);

static void pfunc(const char *, ...);
static void wrongfunc(void);
static int	qfunc(const char *);
static char *gfunc(char *, int);

static char nc_getch(void);
enum ret
{
	SUCCESS, FAILURE, CANCEL
} static nc_gets(char *, int);
static int	colon(void);
static void status(const char *, ...);

static buffer_t *buffer;
static int curline = 1, maxy, maxx, saved = 1;

static void nc_down()
{
	endwin();
}

static void nc_up()
{
	static int init = 0;

	if(!init){
		initscr();
		/*cbreak(); * use raw() to intercept ^C, ^Z */
		raw();
		noecho();

		/* halfdelay() for main-loop style timeout */

		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);

		getmaxyx(stdscr, maxy, maxx);
		--maxy;
		--maxx;

		init = 1;
	}
	refresh();
}

static void status(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	move(maxy, 0);
	clrtoeol();
	vwprintw(stdscr, s, l);
	va_end(l);
}

static void pfunc(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	move(maxy, 0);
	vwprintw(stdscr, s, l);
	va_end(l);
	addch('\n');
}

static int qfunc(const char *s)
{
	int c;
	status("%s", s);
	c = nc_getch();
	return c == '\n' || tolower(c) == 'y';
}

static char *gfunc(char *s, int size)
{
	switch(nc_gets(s, size)){
		case SUCCESS:
			return s;
		case FAILURE:
		case CANCEL:
			break;
	}
	return NULL;
}

static char nc_getch()
{
	int c = getch();
	switch(c){
		case C_CTRL_C:
			sigh(SIGINT);
			break;
	}
	return c;
}

static enum ret nc_gets(char *s, int size)
{
	/*return getnstr(s, len) == OK ? s : NULL;*/
	int x, y, c, count = 0;
	enum ret r;

	getyx(stdscr, y, x);
	clrtoeol();

	do
		switch((c = nc_getch())){
			case C_EOF:
				if(count){
					s[count] = '\0';
					r = SUCCESS;
					goto exit;
				}else{
					r = FAILURE;
					goto exit;
				}

			case C_NEWLINE:
			case '\n':
				if(count < size-1){
					s[count]   = '\n';
					s[count+1] = '\0';
				}else
					s[count] = '\0';
				r = SUCCESS;
				goto exit;

			case C_DEL:
			case C_BACKSPACE:
				if(!count){
					r = CANCEL;
					goto exit;
				}
				count--;
				move(y, --x);
				break;

			case C_ESC:
				r = CANCEL;
				goto exit;

			default:
				if(isprint(c)){
					s[count++] = c;
					addch(c);
					x++;
					if(count >= size-1){
						s[count] = '\0';
						r = SUCCESS;
						goto exit;
					}
				}else{
					r = CANCEL;
					goto exit;
				}
		}
	while(1);

exit:
	move(y, 0);
	return r;
}

static void wrongfunc(void)
{
	move(maxy, 0);
	clrtoeol();
	addch('?');
}

static int colon()
{
#define BUF_SIZE 128
	char in[128], *c;

	(void)mvaddch(maxy, 0, ':');
	switch(nc_gets(in, BUF_SIZE)){
		case SUCCESS:
			c = strchr(in, '\n');
			if(c)
				*c = '\0';

			return runcommand(in, buffer,
					&curline, &saved,
					&wrongfunc, &pfunc,
					&gfunc, &qfunc);

		case FAILURE:
		case CANCEL:
			break;
	}

	return 1;
#undef BUF_SIZE
}

static void sigh(int sig)
{
	nc_down();
	bail(sig);
}

int ncurses_main(const char *filename)
{
	int c;

	signal(SIGINT, &sigh);

	nc_up();

	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			if(errno == ENOENT)
				goto new_file;

			nc_down();
			fprintf(stderr, PROG_NAME": %s: ", filename);
			perror(NULL);
			return 1;
		}else if(nread == 0)
			status("(empty file)\n");
		else
			pfunc("%s: %dC, %dL%s\n", filename,
					buffer_nchars(buffer), buffer_nlines(buffer),
					buffer->haseol ? "" : " (noeol)");
	}else{
new_file:
		buffer = buffer_new();
		status("(new file)");
	}


	do{
		viewbuffer(buffer, curline - 1);
		switch((c = nc_getch())){
			case 'Z':
				if(nc_getch() == 'Z')
					goto exit_while;
				break;

			case ':':
				if(!colon())
					goto exit_while;
				break;

			case C_CTRL_C:
				sigh(SIGINT);
			case C_ESC:
				break;

			default:
				status("unknown: %c (%d)", c, c);
		}
	}while(1);
exit_while:


	nc_down();
	return 0;
}
