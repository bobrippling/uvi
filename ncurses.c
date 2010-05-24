#include <ncurses.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
/* system*/
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>

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

#define pfunc status
static void wrongfunc(void);
static int	qfunc(const char *);
static enum gret gfunc(char *, int);
static void shellout(const char *);

static char nc_getch(void);
static void open(int);
static int	colon(void);
static void status(const char *, ...);

/* extern'd for view.c */
buffer_t *buffer;
int maxy, maxx, saved = 1, curx, cury;

static void nc_down()
{
	view_term();
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
		--maxx; /* TODO: use SIGWINCH */

		curx = cury = 0;

		view_init();

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

	getyx(stdscr, y, x);
	clrtoeol();

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
	if(y < maxy)
		y++;

	move(y, 0);
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
	move(maxy, 0);
	clrtoeol();
	addch('?');
}

static void open(int before)
{
	struct list *cur, *new;

	if(!before)
		move(++cury, (curx = 0));

	cur = list_getindex(buffer_lines(buffer), cury);
	new = command_readlines(&gfunc);

	if(before)
		list_insertlistbefore(cur, new);
	else{
		if(!cur){
			/*
			 * we are appending to the end of the file, hence
			 * line cury doesn't exist yet
			 */
			list_appendlist(buffer_lines(buffer), new);
			cury = list_count(buffer_lines(buffer)) - 1;
		}else
			list_insertlistafter(cur, new);
	}

	saved = 0;
}

static int colon()
{
#define BUF_SIZE 128
	char in[128], *c;

	(void)mvaddch(maxy, 0, ':');
	switch(gfunc(in, BUF_SIZE)){
		case g_CONTINUE:
			c = strchr(in, '\n');
			if(c)
				*c = '\0';

			return command_run(in, buffer,
					&cury, &saved,
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
	int c, changed = 1;

	signal(SIGINT, &sigh);

	nc_up();

  if(!(buffer = command_readfile(filename, pfunc))){
    nc_down();
    fprintf(stderr, PROG_NAME": %s: ", filename);
    perror(NULL);
    return 1;
  }

	do{
		int flag = 0;

		if(changed){
			view_buffer(buffer);
			changed = 0;
		}
		refresh(); /* outside (changed) in case cursor is moved */
		view_move(CURRENT);

		switch((c = nc_getch())){
			case 'Z':
				if(nc_getch() == 'Z')
					/* TODO: check saved */
					goto exit_while;
				break;

			case ':':
				if(!colon())
					goto exit_while;
				changed = 1;
				break;

			case CTRL_AND('g'):
				{
					const int i = list_count(buffer_lines(buffer));

					status("\"%s\"%s %d/%d %.2f%%",
							buffer->fname ? buffer->fname : "(empty file)",
							saved ? "" : " [Modified]", 1+cury,
							i, 100.0f * (float)(1+cury) / (float)i);
					break;
				}

			case 'O':
				flag = 1;
			case 'o':
				open(flag);
				changed = 1;
				break;

			case '0':
				view_move(ABSOLUTE_LEFT);
				break;
			case '$':
				view_move(ABSOLUTE_RIGHT);
				break;
			case 'j':
				view_move(DOWN);
				break;
			case 'k':
				view_move(UP);
				break;
			case 'h':
				view_move(LEFT);
				break;
			case 'l':
				view_move(RIGHT);
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

	buffer_free(buffer);

	nc_down();
	return 0;
}
