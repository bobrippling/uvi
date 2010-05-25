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
#include "list.h"
#include "main.h"
#include "view.h"
#include "alloc.h"

#include "ncurses.h"
#include "config.h"

#define CTRL_AND(c) ((c) & 037)
#define C_ESC			 27
#define C_EOF			 4
#define C_DEL			 127
#define C_BACKSPACE 263
#define C_NEWLINE	 '\r'
#define C_CTRL_C		3

static void initpad(void);
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
static int	colon(void);
static void status(const char *, ...);

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)

/* extern'd for view.c */
buffer_t *buffer;
WINDOW *pad;
int saved = 1, padheight, padwidth, padtop, padleft, padx, pady;
static int gfunc_onpad = 0;

static void nc_down()
{
	delwin(pad);
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

		padx = pady = padtop = padleft = 0;

		init = 1;
	}
	refresh();
}

static void initpad()
{
	padheight = buffer_nlines(buffer) * 2;
	padwidth = COLS;

	if(!padheight)
		padheight = 1;

	pad = newpad(padheight, padwidth);
	if(!pad){
		endwin();
		longjmp(allocerr, 1);
	}
}

static void status(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	move(MAX_Y, 0);
	clrtoeol();
	vwprintw(stdscr, s, l);
	va_end(l);
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

			case C_DEL:
			case C_BACKSPACE:
				if(!count){
					r = g_EOF;
					goto exit;
				}
				count--;
				move(y, --x);
				break;

				/* TODO: CTRL_AND('v') */

			default:
				if(isprint(c)){
					s[count++] = c;
					addch(c);

					x++;
					if(count >= size-1){
						s[count] = '\0';
						r = g_CONTINUE;
						goto exit;
					}
				}else{
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
		wmove(pad, pady, padx = 0);
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

	if(!before)
		wmove(pad, ++pady, (padx = 0));

	cur = buffer_getindex(buffer, pady);
	/*
	 * if !before, then cury has been ++'d,
	 * this means cury is after what we want, hence
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
			 * line cury doesn't exist yet
			 */
			buffer_appendlist(buffer, new);
			pady = buffer_nlines(buffer) - 1;
		}else
			/* Here is what i was talking about before w.r.t. buffer_insertlistafter() */
			buffer_insertlistbefore(buffer, cur, new);
	}

	saved = 0;
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

			return command_run(in, buffer,
					&pady, &saved,
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
	bail(sig);
}

int ncurses_main(const char *filename)
{
	int c, bufferchanged = 1, viewchanged = 1;

	signal(SIGINT, &sigh);

	nc_up();

  if(!(buffer = command_readfile(filename, pfunc))){
		nc_down();
    fprintf(stderr, PROG_NAME": %s: ", filename);
    perror(NULL);
    return 1;
  }

	initpad();

	do{
		int flag = 0;

		if(bufferchanged){
			view_drawbuffer(buffer);
			bufferchanged = 0;
			viewchanged = 1;
		}
		if(viewchanged){
			view_refreshpad(pad);
			viewchanged = 0;
		}
		wmove(pad, pady, padx);

		switch((c = nc_getch())){
			case 'Z':
				if(nc_getch() == 'Z')
					/* TODO: check saved */
					goto exit_while;
				break;

			case ':':
				if(!colon())
					goto exit_while;
				bufferchanged = 1; /* need to review() */
				break;

			case CTRL_AND('g'):
			{
				const int i = buffer_nlines(buffer);

				status("\"%s\"%s %d/%d %.2f%%",
						buffer->fname ? buffer->fname : "(empty file)",
						saved ? "" : " [Modified]", 1+pady,
						i, 100.0f * (float)(1+pady) / (float)i);
				break;
			}

			case 'O':
				flag = 1;
			case 'o':
				open(flag);
				bufferchanged = 1;
				break;

			case '0':
				viewchanged = view_move(ABSOLUTE_LEFT);
				break;
			case '$':
				viewchanged = view_move(ABSOLUTE_RIGHT);
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

	buffer_free(buffer);

	nc_down();
	return 0;
}
