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
		clrtoeol();
		wclrtoeol(pad);
	}else{
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

				/* TODO: CTRL_AND('v') */

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
			kill(getpid(), SIGSTOP);
			nc_up();
			goto get;
#endif
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
	int startx = padx, size = 16, afterlen, enteredlen, tabcount = 0;
	struct list *curline = buffer_getindex(buffer, pady);
	char *line, *linepos, newline = 0, *after = NULL;

	gfunc_onpad = 1;
	linepos = line = umalloc(size);

	if(append && *(char *)curline->data != '\0')
		startx++;

	view_putcursor(pady, startx, 0, 0);
	refresh();

	do{
		int c = nc_getch();

		if(c == C_ESC || c == C_EOF){
			*linepos = '\0';
			break;
		}else if(c == '\n'){
			*linepos = '\0';
			newline = 1;
			break;
		}else if(c == '\b'){
			if(linepos > line){
				if(*linepos == '\t')
					tabcount--;
				view_putcursor(pady, startx, --linepos - line, tabcount);
			}
			continue;
		}else if(c == '\t')
			tabcount++;

		*linepos++ = c;

		if(linepos - line >= size - 1){
			char *new = realloc(line, size *= 2);
			int diff = linepos - line;

			if(!new)
				longjmp(allocerr, ALLOC_NCURSES_1);

			line = new;
			linepos = line + diff;
			linepos[1] = '\0';
		}

		view_waddch(stdscr, c);
		refresh();
	}while(1);

	enteredlen = linepos - line;

	linepos = realloc(curline->data,
	                  strlen(curline->data) +
	                  enteredlen + 1);
	if(!linepos)
		longjmp(allocerr, ALLOC_NCURSES_2);

	curline->data = linepos;

	linepos += startx; /* linepos is now the insertion point */
	afterlen = strlen(linepos);

	if(afterlen)
		if(newline){
			after = linepos + 1;
			memmove(after, after - 1, afterlen); /* no more '\0' */
			after[-1] = '\0';
		}else
			memmove(linepos + enteredlen, linepos, afterlen + 1); /* include the nul char */
	else
		linepos[enteredlen] = '\0'; /* vital as the above +1 */

	if(newline){
		struct list *last;
		char *tmp;
		const int savedy = pady;
		int y;

		y = open(0);
		last = buffer_getindex(buffer, savedy + y);

		tmp = realloc(last->data, strlen(last->data) + afterlen + 1);
		if(!tmp)
			longjmp(allocerr, ALLOC_NCURSES_3);

		last->data = tmp;
		tmp += strlen(last->data);

		memmove(tmp, after, afterlen);
		tmp[afterlen] = '\0';
	}

	memmove(linepos, line, enteredlen + newline); /* include '\0' */
	free(line);

	buffer_modified(buffer) = 1;
	padx = startx + enteredlen - 1;
	if(padx < 0)
		padx = 0;
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

		/* _remove_range expects 1-based offsets */
		r.start = pady + 1;
		r.end   = y    + 1;

		if(islinemotion(mparam)){
			/* delete lines between pady and y, inclusive */
			buffer_remove_range(buffer, &r);
		}else{
			/*
			 * delete chars between (x,y) and (padx,pady), inclusive
			 * aka delete between y+1 and pady-1, then join y & pady into one
			 */
			r.start++;
			r.end--;
			if(r.start <= r.end)
				/* lines to remove */
				buffer_remove_range(buffer, &r);

			/* join line pady with line y */
			join(1);
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

	if(pady + 1 + ntimes >= buffer_nlines(buffer)){
		status("can't join %d line%s", ntimes,
				ntimes > 1 ? "s" : "");
		return;
	}

	r.end = (r.start = pady + 2) + ntimes - 1;
	/*
	 * +2, because...
	 * +1 for the next line
	 * +1 because buffer_extract_range expects
	 * a 1-based offset
	 */

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
					&pady, &buffer,
					&wrongfunc, &pfunc,
					&gfunc, &qfunc, &shellout);

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
	command_dumpbuffer(buffer);
	buffer_free(buffer);
	bail(sig);
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
		switch((c = nc_getch())){
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
				if(buffer_readonly(buffer))
					status(READ_ONLY_ERR);
				else{
					open(flag);
					bufferchanged = 1;
					SET_DOT();
				}
				break;

			case 'X':
				SET_MOTION(MOTION_BACKWARD_LETTER);
				delete(&motion);
				bufferchanged = 1;
				SET_DOT();
				break;
			case 'x':
				SET_MOTION(MOTION_FORWARD_LETTER);
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
