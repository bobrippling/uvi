#include <ncurses.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
/* system*/
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>

#include "range.h"
#include "buffer.h"
#include "command.h"
#include "util/list.h"
#include "main.h"
#include "view.h"
#include "util/alloc.h"
#include "vars.h"

#include "ncurses.h"
#include "config.h"

#define CTRL_AND(c)	((c) & 037)
#define C_ESC				27
#define C_EOF				4
#define C_DEL				127
#define C_BACKSPACE	263
#define C_ASCIIDEL	7
#define C_CTRL_C		3
#define C_TAB				9
#define C_NEWLINE		'\r'

static void nc_up(void);
static void nc_down(void);
static void sigh(int);

#define pfunc status
static void wrongfunc(void);
static int	qfunc(const char *);
static enum gret gfunc(char *, int);
static void shellout(const char *);

static char nc_getch(void);
static void open(int);
static int colon(void);
static void status(const char *, ...);
static void showpos(void);

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)

/* extern'd for view.c */
buffer_t *buffer;
WINDOW *pad;
int padheight, padwidth, padtop, padleft, padx, pady;
static int gfunc_onpad = 0, pfunc_wantconfimation = 0;

extern int debug;


static void nc_down()
{
	view_termpad();
	pad = NULL;
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

		pady = padx = padtop = padleft = 0;

		init = 1;
	}
	refresh();
}

static void status(const char *s, ...)
{
	va_list l;

	move(MAX_Y, 0);

	if(strchr(s, '\n') || pfunc_wantconfimation){
		/*
		 * print a bunch of lines,
		 * and tell main() to wait for input from the user
		 */
		move(pfunc_wantconfimation++, 0);
	}

	clrtoeol();
	va_start(l, s);
	vwprintw(stdscr, s, l);
	va_end(l);
}

static void showpos()
{
	const int i = buffer_nlines(buffer);

	status("\"%s\"%s %d/%d %.2f%%",
			buffer_filename(buffer),
			buffer_modified(buffer) ? " [Modified]" : "", 1+pady,
			i, 100.0f * (float)(1+pady) / (float)i);
}

static int qfunc(const char *s)
{
	int c;
	status("%s", s);
	c = nc_getch();
	return c == '\n' || tolower(c) == 'y';
}

static void shellout(const char *cmd)
{
	int ret;

	endwin();

	ret = system(cmd);
	if(ret == -1)
		perror("system()");
	else
		printf("\"%s\" returned %d\n", cmd, WEXITSTATUS(ret));

	fputs("Press enter to continue...", stdout);
	fflush(stdout);
	ret = getchar();
	if(ret != '\n' && ret != EOF)
		while((ret = getchar()) != '\n' && ret != EOF);

	refresh();
}

static enum gret gfunc(char *s, int size)
{
	/*return getnstr(s, len) == OK ? s : NULL;*/
	int x, y, c, count = 0;
	enum gret r;

	/* if we're on a pad, draw over the top of it, onto stdscr */
	if(gfunc_onpad){
		getyx(pad, y, x);
		x -= padleft;
		y -= padtop;
		move(y, x);
		clrtoeol();
	}else{
		getyx(stdscr, y, x);
		clrtoeol();
	}

	do
		switch((c = nc_getch())){
			case C_ESC:
				r = g_LAST;
				goto exit;

			case C_NEWLINE:
			case '\n':
				r = g_CONTINUE;
				goto exit;

			case '\b': /* backspace */
				if(!count){
					r = g_EOF;
					goto exit;
				}
				count--;
				move(y, --x);
				break;

				/* TODO: CTRL_AND('v') */

			default:
				if(isprint(c) || c == '\t'){
					s[count++] = c;

					/* FIXME - use view_addch() */
					if(c == '\t')
						addch(' ');
					else
						addch(c);

					x++;
					if(count >= size-1){
						s[count] = '\0';
						r = g_CONTINUE;
						goto exit;
					}
				}else{
					status("unrecognised character %d\n", c);
					r = g_LAST;
					goto exit;
				}
		}
	while(1);

exit:
	if(count < size-1){
		s[count]   = '\n';
		s[count+1] = '\0';
	}else
		s[count] = '\0';

	addch('\n');
	move(++y, 0);
	if(gfunc_onpad){
		++pady;
		padx = 0;
		view_updatecursor();
	}

	return r;
}

static char nc_getch()
{
	int c = getch();
	switch(c){
		case C_CTRL_C:
			sigh(SIGINT);
			break;

		case C_EOF:
			return EOF;

		case C_DEL:
		case C_BACKSPACE:
		case C_ASCIIDEL:
			return '\b';

		case C_NEWLINE:
			return '\n';

		case C_TAB:
			return '\t';
	}
	return c;
}

static void wrongfunc(void)
{
	move(MAX_Y, 0);
	clrtoeol();
	addch('?');
}

static void open(int before)
{
	struct list *cur, *new;

	if(!before){
		++pady;
		padx = 0;
		view_updatecursor();
	}

	cur = buffer_getindex(buffer, pady);
	/*
	 * if !before, then pady has been ++'d,
	 * this means pady is after what we want, hence
	 * hence why buffer_insertlistafter() isn't used below
	 */

	gfunc_onpad = 1;
	new = command_readlines(&gfunc);

	if(before)
		buffer_insertlistbefore(buffer, cur, new);
	else{
		if(!cur){
			/*
			 * we are appending to the end of the file, hence
			 * line pady doesn't exist yet
			 */
			buffer_appendlist(buffer, new);
			pady = buffer_nlines(buffer) - 1;
		}else
			/* Here is what i was talking about before w.r.t. buffer_insertlistafter() */
			buffer_insertlistbefore(buffer, cur, new);
	}
	if(pady > 0){
		if(cur) /* cur is null if we added to the last line */
			pady--; /* stay on the line(s) we just inserted */
		else if(pady > MAX_Y)
			/* last line - shift the pad up if needed */
			padtop++;
	}

	buffer_modified(buffer) = 1;
}

static int colon()
{
#define BUF_SIZE 128
	char in[BUF_SIZE], *c;

	(void)mvaddch(MAX_Y, 0, ':');
	gfunc_onpad = 0;
	switch(gfunc(in, BUF_SIZE)){
		case g_CONTINUE:
			c = strchr(in, '\n');
			if(c)
				*c = '\0';

			return command_run(in,
					&pady, buffer,
					&wrongfunc, &pfunc,
					&gfunc, &qfunc, &shellout);


		case g_LAST: /* esc means cancel in this sense */
		case g_EOF:
			break;
	}

	return 1;
#undef BUF_SIZE
}

static void sigh(int sig)
{
	nc_down();
	command_dumpbuffer(buffer);
	buffer_free(buffer);
	bail(sig);
}

int ncurses_main(const char *filename, char readonly)
{
	int c, bufferchanged = 1, viewchanged = 1, ret = 0;
	void (*oldinth)(int), (*oldsegh)(int);

	nc_up();

  if(!(buffer = command_readfile(filename, readonly, pfunc))){
		/* TODO: leave ncurses up, with empty buffer */
		nc_down();
    fprintf(stderr, PROG_NAME": %s: ", filename);
    perror(NULL);
		ret = 1;
		goto fin;
  }

	/* have to be after buffer's been initialised */
	if(!debug){
		oldinth = signal(SIGINT, &sigh);
		oldsegh = signal(SIGSEGV, &sigh);
	}

	view_initpad();

	do{
		int flag = 0;

		if(pfunc_wantconfimation){
			pfunc_wantconfimation = 0;
			status("press any key...");
			c = nc_getch();
			if(c == ':')
				goto colon;
		}

		if(bufferchanged){
			view_drawbuffer(buffer);
			bufferchanged = 0;
			viewchanged = 1;
		}
		if(viewchanged){
			view_refreshpad(pad);
			view_updatecursor();
			viewchanged = 0;
		}
		/*
		 * view.c handles cursor positioning
		 * either way, cursor placement should be _before_
		 * view_refreshpad
		 */

		switch((c = nc_getch())){
			case ':':
colon:
				if(!colon())
					goto exit_while;
				bufferchanged = 1; /* need to view_refresh_or_whatever() */
				break;

			case CTRL_AND('g'):
				showpos();
				break;

			case 'O':
				flag = 1;
			case 'o':
				if(buffer_readonly(buffer))
					status("file is read-only");
				else{
					open(flag);
					bufferchanged = 1;
				}
				break;

			case CTRL_AND('e'):
				viewchanged = view_scroll(SINGLE_DOWN);
				break;
			case CTRL_AND('y'):
				viewchanged = view_scroll(SINGLE_UP);
				break;


			case 'g':
				viewchanged = view_move(ABSOLUTE_UP);
				break;
			case 'G':
				viewchanged = view_move(ABSOLUTE_DOWN);
				break;
			case '0':
				viewchanged = view_move(ABSOLUTE_LEFT);
				break;
			case '$':
				viewchanged = view_move(ABSOLUTE_RIGHT);
				break;
			case '^':
				viewchanged = view_move(NO_BLANK);
				break;
			case 'j':
				viewchanged = view_move(DOWN);
				break;
			case 'k':
				viewchanged = view_move(UP);
				break;
			case 'h':
				viewchanged = view_move(LEFT);
				break;
			case 'l':
				viewchanged = view_move(RIGHT);
				break;

			case C_ESC:
				break;

			default:
				status("unknown: %c (%d)", c, c);
		}
	}while(1);
exit_while:

	nc_down();
fin:
	view_termpad();
	buffer_free(buffer);
	signal(SIGINT,  oldinth);
	signal(SIGSEGV, oldsegh);

	return ret;
}
