/* kill */
#define _POSIX_SOURCE
#include <ncurses.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <limits.h>
/* system */
#include <stdlib.h>
#include <sys/wait.h>

#include "../config.h"

#include "../range.h"
#include "../buffer.h"
#include "../command.h"
#include "../util/list.h"
#include "../main.h"
#include "motion.h"
#include "view.h"
#include "../util/alloc.h"
#include "../vars.h"
#include "marks.h"

#include "ncurses.h"

extern struct settings global_settings;

#define NCURSES_DEBUG_SHOW_UNKNOWN	0
#define NCURSES_DEBUG_SHOWXY				0

#define NC_RAW				0

#define CTRL_AND(c)		((c) & 037)
#define CTRL_AND_g		8
#define C_ESC					27
#define C_EOF					4
#define C_DEL					127
#define C_BACKSPACE		263
#define C_ASCIIDEL		7
#define C_CTRL_C			3
#define C_CTRL_Z			26
#define C_TAB					9
#define C_NEWLINE			'\r'

#if VIEW_COLOUR
#define coloron(i)   (attron(COLOR_PAIR(i)))
#define coloroff(i)  (attroff(COLOR_PAIR(i)))
#endif
#define iswordchar(c) (isalnum(c) || c == '_')

static void nc_up(void);
static void nc_down(void);
static void nc_toggle(char up);
static void sigh(int);

/* I/O */
#define pfunc status
static void wrongfunc(void);
static int  qfunc(const char *, ...);
static enum gret gfunc(char *, int);
static void shellout(const char *);
static int  nc_getch(void);

/* text altering */
static void insert(int);
static int  open(int); /* as in, 'o' & 'O' */
static void delete(struct motion *);
static void join(int);
static void shift(unsigned int, char);
static void tilde(int);
static void replace(int);

/* extra */
static int  colon(void);
static void showgirl(unsigned int);
static int  search(int);

static void unknownchar(int);
static char iseditchar(int);

static void status(const char *, ...);
static void vstatus(const char *, va_list);
static void showpos(void);

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)

/* extern'd for view.c */
buffer_t *buffer;
WINDOW *pad;
int padheight, padwidth, padtop, padleft, padx, pady;

static int gfunc_onpad = 0, pfunc_wantconfimation = 0;
#define SEARCH_STR_SIZE 256
static char searchstr[SEARCH_STR_SIZE] = { 0 };

extern int debug;

static int search(int next)
{
	char *pos;
	int y = pady + 1;
	struct list *l = buffer_getindex(buffer, y);

	if(next){
		if(*searchstr)
			goto dosearch;
		else{
			status("no previous search");
			return 0;
		}
	}

	gfunc_onpad = 0;
	mvaddch(MAX_Y, 0, '/');
	switch(gfunc(searchstr, SEARCH_STR_SIZE)){
		case g_LAST:
		case g_EOF:
			status("search aborted");
			break;

		case g_CONTINUE:
			if((pos = strchr(searchstr, '\n')))
				*pos = '\0';

dosearch:
			while(l){
				if((pos = strstr(l->data, searchstr))){
					padx = pos - (char *)l->data;
					pady = y;
					view_cursoronscreen();
					return 1;
				}

				y++;
				l = l->next;
			}
			status("\"%s\" not found", searchstr);
			break;
	}

	return 0;
}

void shift(unsigned int nlines, char indent)
{
	struct list *l = buffer_getindex(buffer, pady);

	if(!nlines)
		nlines = 1;

	while(nlines-- && l){
		char *data = l->data;

		if(indent){
			int len = strlen(data) + 1;
			char *new = realloc(data, len + 1);

			if(!new)
				longjmp(allocerr, 1);

			l->data = new;

			memmove(new+1, new, len);
			*new = '\t';
		}else
			switch(*data){
				case ' ':
					if(data[1] == ' ')
						memmove(data, data+2, strlen(data+1));
					break;

				case '\t':
					memmove(data, data+1, strlen(data));
					break;
			}

		l = l->next;
	}

	buffer_modified(buffer) = 1;
}

void tilde(int rep)
{
	char *data = (char *)buffer_getindex(buffer, pady)->data,
			*pos = data + padx;

	while(rep--){
		if(islower(*pos))
			*pos = toupper(*pos);
		else
			*pos = tolower(*pos);

		mvwaddch(pad, pady, view_getactualx(pady, padx), *pos);

		/**pos ^= (1 << 5); * flip bit 100000 = 6 */

		if(padx < (signed)strlen(data))
			padx++;
		else
			break;

		pos++;
	}

	view_cursoronscreen();
	buffer_modified(buffer) = 1;
}

void showgirl(unsigned int page)
{
	char *line = buffer_getindex(buffer, pady)->data, *wordstart, *wordend, *word;
	int len;

	if(page > 9){
		status("invalid man page");
		return;
	}

	wordend = wordstart = line + padx;

	if(!iswordchar(*wordstart)){
		status("invalid word");
		return;
	}

	while(--wordstart >= line && iswordchar(*wordstart));
	wordstart++;

	while(iswordchar(*wordend))
		wordend++;

	len = wordend - wordstart;

	word = umalloc(len + 7);
	if(page)
		sprintf(word, "man %d ", page);
	else
		strcpy(word, "man ");

	strncat(word, wordstart, len);
	word[len + 6] = '\0';

	shellout(word);
	free(word);
}

void replace(int n)
{
	int c;
	struct list *cur = buffer_getindex(buffer, pady);
	char *s = cur->data;

	if(n <= 0)
		n = 1;

	c = nc_getch();

	if(*s == '\0')
		return;
	else if(n > (signed)strlen(s + padx))
		return;


	if(isprint(c)){
		buffer_modified(buffer) = 1;

		while(n--)
			s[padx + n] = c;

	}else if(c == '\n'){
		/* delete n chars, and insert 1 line */
		char *off = s + padx + n-1;
		char *cpy = umalloc(strlen(off));

		buffer_modified(buffer) = 1;

		memset(off - n + 1, '\0', n);
		strcpy(cpy, off + 1);

		buffer_insertafter(buffer, cur, cpy);

		pady++;
		padx = 0;
	}else
		unknownchar(c);
}

static void nc_toggle(char up)
{
	if(up)
		nc_up();
	else
		endwin();
}

static void nc_down()
{
	view_termpad();
	pad = NULL;
	endwin();
}

static void nc_up()
{
	static char init = 0;

	if(!init){
		initscr();
		noecho();
		cbreak();
#if NC_RAW
		raw(); /* use raw() to intercept ^C, ^Z */
#endif

		/* halfdelay() for main-loop style timeout */

		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);

		/*ESCDELAY = 25; * duh **/

		pady = padx = padtop = padleft = 0;

		init = 1;
	}

	refresh();
}

static void vstatus(const char *s, va_list l)
{
	if(strchr(s, '\n') || pfunc_wantconfimation)
		/*
		 * print a bunch of lines,
		 * and tell main() to wait for input from the user
		 */
		move(pfunc_wantconfimation++, 0);
	else
		move(MAX_Y, 0);

	clrtoeol();
#if VIEW_COLOUR
	if(global_settings.colour)
		coloron(COLOR_RED);
#endif
	vwprintw(stdscr, s, l);
#if VIEW_COLOUR
	if(global_settings.colour)
		coloroff(COLOR_RED);
#endif
}

void status(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	vstatus(s, l);
	va_end(l);
}

static void showpos()
{
	const int i = buffer_nlines(buffer);

	status("\"%s\"%s %d/%d %.2f%%",
			buffer_filename(buffer),
			buffer_modified(buffer) ? " [Modified]" : "", 1 + pady,
			i, 100.0f * (float)(1 + pady) /(float)i);
}

static int qfunc(const char *s, ...)
{
	va_list l;
	int c;

	va_start(l, s);
	vstatus(s, l);
	va_end(l);

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
	else if(WEXITSTATUS(ret))
		printf("%s returned %d\n", cmd, WEXITSTATUS(ret));

	fputs("Press enter to continue...", stdout);
	fflush(stdout);
	ret = getchar();

	if(ret != '\n' && ret != EOF)
		while((ret = getchar()) != '\n' && ret != EOF);

	refresh();
}

static enum gret gfunc(char *s, int size)
{
	/*return getnstr(s, len) == OK ? s : NULL; */
	int x, y, c, count = 0;
	enum gret r;

	/* if we're on a pad, draw over the top of it, onto stdscr */
	if(gfunc_onpad){
		getyx(pad, y, x);
		x -= padleft;
		y -= padtop;
		move(y, x);
		/*clrtoeol();*/
		/*wclrtoeol(pad);*/
	}else{
		/* probably a command */
		clrtoeol();

		getyx(stdscr, y, x);
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
				if(gfunc_onpad)
					move(y, view_getactualx(y, --x));
				else
					move(y, --x);
				break;

			default:
				if(isprint(c) || c == '\t'){
					s[count++] = c;

					view_waddch(stdscr, c);
					wrefresh(stdscr);
					x++;

					if(count >= size - 1){
						s[count] = '\0';
						r = g_CONTINUE;
						goto exit;
					}
				}else{
					unknownchar(c);
					r = g_LAST;
					goto exit;
				}
		}
	while(1);

exit:
	if(count < size - 1){
		s[count]   = '\n';
		s[count+1] = '\0';
	}else
		s[count] = '\0';

	if(gfunc_onpad){
		waddnstr(pad, s, MAX_X);
		waddch(pad, '\n');
	}else{
		addch('\n');
		move(++y, 0);
	}

	if(gfunc_onpad){
		++pady;
		padx = 0;
		view_cursoronscreen();
	}

	return r;
}

static int nc_getch()
{
	int c;


#if NC_RAW
get:
#endif
	c = getch();
	switch(c){
#if NC_RAW
		case C_CTRL_C:
			sigh(SIGINT);
			break;

		case C_CTRL_Z:
			nc_down();
			raise(SIGSTOP);
			nc_up();
			goto get;
#endif

		/* TODO: CTRL_AND('v') */

		case 0:
		case -1:
		case C_EOF:
			return CTRL_AND('d');

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

static void unknownchar(int c)
{
	status("unrecognised character #%d", c);
}


static void insert(int append)
{
	struct list *listpos = buffer_getindex(buffer, pady),
							*new, *firstline;
	char *newline;
	int savepadx;
	int newlen;

	gfunc_onpad = 1;

	if(append && *(char *)listpos->data != '\0'){
		padx++;
		view_cursoronscreen();
	}

	savepadx = padx;
	firstline = new = command_readlines(&gfunc);
	padx = savepadx;
	pady--;

	new = new->next;

	/* snip */
	firstline->next = NULL;
	if(new)
		new->prev = NULL;

	/*
	 * insert firstline->data into (pady, padx)
	 * then append new (if !NULL) onto listpos
	 */

	newline = realloc(listpos->data, strlen(listpos->data) + strlen(firstline->data) + 1);
	if(!newline)
		longjmp(allocerr, 1);
	listpos->data = newline;

	newlen = strlen(firstline->data);

	memmove(newline + padx + newlen, newline + padx, strlen(newline + padx) + 1);
	memcpy(newline + padx, firstline->data, newlen);

	list_free(firstline);

	if(new){
		/* cut off the bit after firstline->data and append to last */
		struct list *last = list_gettail(new);
		char *pos = newline + padx + newlen, *realloced;

		realloced = realloc(last->data, strlen(last->data) + strlen(pos) + 1);
		if(!realloced)
			longjmp(allocerr, 1);
		last->data = realloced;

		strcat(realloced, pos);
		*pos = '\0';

		buffer_insertlistafter(buffer, listpos, new);
	}

	buffer_modified(buffer) = 1;

	padx += newlen - 1;
}

/* returns nlines */
static int open(int before)
{
	struct list *cur, *new;
	int nlines;

	if(!before)
		++pady;

	wmove(pad, pady, padx = 0);
	clrtoeol();

	cur = buffer_getindex(buffer, pady);
	/*
	 * if !before, then pady has been ++'d,
	 * this means pady is after what we want, hence
	 * hence why buffer_insertlistafter() isn't used below
	 */

	gfunc_onpad = 1;
	new = command_readlines(&gfunc);

	nlines = list_count(new);

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

	return nlines;
}

static void delete(struct motion *mparam)
{
	struct bufferpos topos;
	struct screeninfo si;
	int x = padx, y = pady;

	topos.buffer = buffer;
	topos.x = &x;
	topos.y = &y;
	si.padtop = padtop;
	si.padheight = MAX_Y;

	if(applymotion(mparam, &topos, &si)){
		struct range r;

		if(pady > y){
			int t = y;
			y = pady;
			pady = t;
		}

		r.start = pady;
		r.end   = y;

		if(islinemotion(mparam)){
			/* delete lines between pady and y, inclusive */
			buffer_remove_range(buffer, &r);
			if(pady >= buffer_nlines(buffer))
				pady = buffer_nlines(buffer) - 1;
		}else{
			char *data = buffer_getindex(buffer, pady)->data, forwardmotion = 1;
			int oldlen = strlen(data);

			if(r.start < r.end){
				/* lines to remove */
				r.start++;
				buffer_remove_range(buffer, &r);

				/* join line pady with line y */
				join(1);
				x += oldlen;

				data = buffer_getindex(buffer, pady)->data;
			}

			/* padx should be left-most */
			if(padx > x){
				int t = x;
				x = padx;
				padx = t;
				forwardmotion = 0;
			}

			{
				char *linestart = data;
				char *curpos    = linestart + padx;
				char *xpos      = linestart + x;

				if(forwardmotion){
					if(*xpos != '\0' && !istilmotion(mparam))
						xpos++; /* delete up to and including where we motion() to */
				}else{
					if(*xpos != '\0'){
						curpos++;
						/* include the current char in the deletion */
						xpos++;
					}
				}

				/* remove the chars between padx and x, inclusive */
				memmove(curpos, xpos, strlen(xpos) + 1);

				if(padx >= x)
					padx = x;
			}
		}
	}

	buffer_modified(buffer) = 1;
}

static void join(int ntimes)
{
	struct list *jointhese, *l, *cur = buffer_getindex(buffer, pady);
	struct range r;
	char *alloced;
	int len = 0;

	if(ntimes == 0)
		ntimes = 1;

	if(pady + ntimes >= buffer_nlines(buffer)){
		status("can't join %d line%s", ntimes,
				ntimes > 1 ? "s" : "");
		return;
	}

	r.start = pady + 1; /* extract the next line(s) */
	r.end   = r.start + ntimes;

	jointhese = buffer_extract_range(buffer, &r);

	for(l = jointhese; l; l = l->next)
		len += strlen(l->data);

	alloced = realloc(cur->data, strlen(cur->data) + len + 1);
	if(!alloced)
		longjmp(allocerr, 1);

	cur->data = alloced;
	for(l = jointhese; l; l = l->next)
		/* TODO: add a space between these? */
		strcat(cur->data, l->data);

	list_free(jointhese);

	buffer_modified(buffer) = 1;
}

static int colon()
{
#define BUF_SIZE 128
	char in[BUF_SIZE], *c;
	int ret;

	(void)mvaddch(MAX_Y, 0, ':');
	gfunc_onpad = 0;

	switch(gfunc(in, BUF_SIZE)){
		case g_CONTINUE:
			c = strchr(in, '\n');

			if(c)
				*c = '\0';

			ret = command_run(in,
					&pady, &buffer, &wrongfunc, &pfunc,
					&gfunc, &qfunc, &shellout, &nc_toggle);

			return ret;

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

static char iseditchar(int c)
{
	switch(c){
		case 'O':
		case 'o':
		case 'X':
		case 'x':
		case 'C':
		case 'D':
		case 'c':
		case 'd':
		case 'A':
		case 'a':
		case 'i':
		case 'I':
		case 'J':
		case 'r':
		case '>':
		case '<':
		case '~':
			return 1;
	}
	return 0;
}

int ncurses_main(const char *filename, char readonly)
{
	int c, bufferchanged = 1, viewchanged = 1, ret = 0, multiple = 0,
			prevcmd = '\0', prevmultiple = 0;
	void (*oldinth)(int), (*oldsegh)(int);
	struct motion motion;

	if(!isatty(STDIN_FILENO))
		fputs(PROG_NAME": warning: input is not a terminal\n", stderr);
	if(!isatty(STDOUT_FILENO))
		fputs(PROG_NAME": warning: output is not a terminal\n", stderr);

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
		int flag = 0, resetmultiple = 1;

#if NCURSES_DEBUG_SHOWXY
		status("at (%d, %d): %c", padx, pady, ((char *)buffer_getindex(buffer, pady)->data)[padx]);
		view_updatecursor();
#endif
		if(pfunc_wantconfimation){
			status("---");
			pfunc_wantconfimation = 0;
			status("press any key...");
			ungetch(nc_getch());
		}
		if(bufferchanged){
			view_drawbuffer(buffer);
			bufferchanged = 0;
			viewchanged = 1;
		}
		if(viewchanged){
			/*
			 * cursor must be updated before
			 * the pad is refreshed
			 */
			view_cursoronscreen();
			viewchanged = 0;
		}
		/*
		 * view.c handles cursor positioning
		 * either way, cursor placement should be _before_
		 * view_refreshpad
		 */

#define INC_MULTIPLE() \
				do \
					if(multiple < INT_MAX/10){ \
						multiple = multiple * 10 + c - '0'; \
						resetmultiple = 0; \
					}else \
						pfunc("range too large"); \
						/*resetmultiple = 1;*/ \
				while(0)

#define SET_DOT() do{ \
					prevcmd = c; \
					prevmultiple = multiple; \
				}while(0)

#define SET_MOTION(m) do{ \
					motion.motion = m; \
					motion.ntimes = multiple; \
				}while(0)

switch_start:
		c = nc_getch();
		if(iseditchar(c) && buffer_readonly(buffer)){
			status(READ_ONLY_ERR);
			view_cursoronscreen();
			continue;
		}

		switch(c){
			case ':':
				if(!colon())
					goto exit_while;

				/* need to view_refresh_or_whatever() */
				bufferchanged = 1;
				break;

			case '.':
				if(prevcmd){
					ungetch(prevcmd);
					multiple = prevmultiple;
					goto switch_start;
				}else
					pfunc("no previous command");
				break;

			case 'm':
				c = nc_getch();
				if(validmark(c))
					mark_set(c, pady, padx);
				else
					status("invalid mark");
				break;

			case CTRL_AND_g:
				showpos();
				break;

			case 'O':
				flag = 1;
			case 'o':
				open(flag);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'X':
				SET_MOTION(MOTION_BACKWARD_LETTER); /* TODO: test */
				delete(&motion);
				bufferchanged = 1;
				SET_DOT();
				break;
			case 'x':
				SET_MOTION(MOTION_NO_MOVE);
				delete(&motion);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'C':
				flag = 1;
			case 'D':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				delete(&motion);
				if(flag)
					insert(1 /* append */);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'c':
				flag = 1;
			case 'd':
				c = nc_getch();
				if(c == 'd' || c == 'c'){
					/* allow dd or cc (or dc or cd... but yeah) */
					SET_MOTION(MOTION_LINE);
				}else{
					ungetch(c);
					getmotion(&status, &nc_getch, &motion);
				}
				if(motion.motion != MOTION_UNKNOWN){
					delete(&motion);
					view_drawbuffer(buffer);
					if(flag)
						insert(0);
					bufferchanged = 1;
					SET_DOT();
				}
				break;

			case 'A':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				view_move(&motion);
			case 'a':
				flag = 1;
			case 'i':
case_i:
				insert(flag);
				bufferchanged = 1;
				SET_DOT();
				break;
			case 'I':
				SET_MOTION(MOTION_NOSPACE);
				view_move(&motion);
				goto case_i;

			case 'J':
				join(multiple);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'r':
				replace(multiple);
				bufferchanged = 1;
				SET_DOT();
				break;

			case CTRL_AND('f'):
				viewchanged = view_scroll(PAGE_DOWN);
				break;
			case CTRL_AND('b'):
				viewchanged = view_scroll(PAGE_UP);
				break;
			case CTRL_AND('d'):
				viewchanged = view_scroll(HALF_DOWN);
				break;
			case CTRL_AND('u'):
				viewchanged = view_scroll(HALF_UP);
				break;
			case CTRL_AND('e'):
				viewchanged = view_scroll(SINGLE_DOWN);
				break;
			case CTRL_AND('y'):
				viewchanged = view_scroll(SINGLE_UP);
				break;

			case 'n':
				flag = 1;
			case '/':
				viewchanged = search(flag);
				break;

			case '>':
				flag = 1;
			case '<':
				shift(multiple, flag);
				bufferchanged = 1;
				SET_DOT();
				break;

			case '~':
				tilde(multiple ? multiple : 1);
				SET_DOT();
				break;

			case 'K':
				showgirl(multiple);
				break;

			case 'z':
				/* screen move - vim's zz, zt & zb */
				switch(nc_getch()){
					case 'z':
						view_scroll(CURSOR_MIDDLE);
						break;
					case 't':
						view_scroll(CURSOR_TOP);
						break;
					case 'b':
						view_scroll(CURSOR_BOTTOM);
						break;
				}
				break;

			case C_ESC:
				break;

			default:
				if(isdigit(c) && (c == '0' ? multiple : 1))
					INC_MULTIPLE();
				else{
					ungetch(c);
					motion.ntimes = multiple;
					getmotion(&status, &nc_getch, &motion);

					if(motion.motion != MOTION_UNKNOWN)
						view_move(&motion);
				}
		}

		if(resetmultiple)
			multiple = 0;
	}while(1);

exit_while:

	nc_down();
fin:
	view_termpad();
	buffer_free(buffer);
	command_free();

	/*
	 * apparently this uses uninitialised memory
	 * signal(SIGINT,  oldinth);
	 * signal(SIGSEGV, oldsegh);
	 */

	return ret;
#undef INC_MULTIPLE
#undef SET_DOT
}
