/*
 * view.c handles displaying the buffer,
 * as opposed to ncurses.c, which handles
 * user i/o and commands
 */
#include <ncurses.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <ctype.h>

#include "../util/list.h"
#include "../range.h"
#include "../buffer.h"
#include "motion.h"
#include "view.h"
#include "../util/alloc.h"

#include "../config.h"

extern struct settings global_settings;

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)

static void clippadx(void);
static void padyinrange(void);
static void view_updatecursor(void);


static void resizepad(void);
static int view_actualx(int, int);
static int padlim_top = 0;

WINDOW *pad;
int padheight, padwidth;


extern int padtop, padleft, padx, pady;

extern buffer_t *buffer;

int view_scroll(enum scroll s)
{
	int ret = 0;

	switch(s) {
		case SINGLE_DOWN:

			if(padtop < buffer_nlines(buffer) - 1) {
				padtop++;

				if(pady <= padtop) {
					pady = padtop;
					clippadx();
				}

				ret = 1;
			}

			break;
		case SINGLE_UP:

			if(padtop) {
				padtop--;

				if(pady >= padtop + MAX_Y) {
					pady = padtop + MAX_Y - 1;
					clippadx();
				}

				ret = 1;
			}

			break;
		case PAGE_UP:
			padtop -= MAX_Y;

			if(padtop < 0)
				padtop = 0;

			padyinrange();
			ret = 1;
			break;
		case PAGE_DOWN:
			padtop += MAX_Y;
			ret = buffer_nlines(buffer) - 1;

			if(padtop + MAX_Y > ret)
				clear();

			if(padtop > ret)
				padtop = ret;

			padyinrange();
			ret = 1;
			break;
		case HALF_UP:
			padtop -= MAX_Y / 2;

			if(padtop < 0)
				padtop = 0;

			padyinrange();
			ret = 1;
			break;
		case HALF_DOWN:
			padtop += MAX_Y / 2;
			ret = buffer_nlines(buffer) - 1;

			if(padtop + MAX_Y > ret)
				clear();

			if(padtop > ret)
				padtop = ret;

			padyinrange();
			ret = 1;
			break;
		case CURSOR_TOP:
			padtop = pady;
			break;
		case CURSOR_BOTTOM:

			if((padtop = pady - MAX_Y + 1) < 0)
				padtop = 0;

			break;
		case CURSOR_MIDDLE:

			if((padtop = pady - MAX_Y / 2) < 2)
				padtop = 0;

			break;
	}

	view_updatecursor();
	return ret;
}

static void padyinrange()
{
	if(pady < padtop)
		pady = padtop;
	else if(pady > padtop + MAX_Y)
		pady = padtop + MAX_Y - 1;

	clippadx();
}

int view_getactualx(int y, int x)
{
	struct list *pos = buffer_getindex(buffer, y);
	char *data;
	int actualx = 0;

	if(!pos)
		return x;

	data = pos->data;

	while(*data && x > 0) {
		if(*data++ == '\t')
			if(global_settings.showtabs)
				actualx += 2;
			else
				actualx += global_settings.tabstop;
		else
			actualx++;

		--x;
	}

	return actualx + x;
}

static int view_actualx(int y, int x)
{
	struct list *l = buffer_getindex(buffer, y);

	if(l) {
		char *data = l->data;
		int actualx = 0;

		while(*data && x -- > 0)
			if(*data++ == '\t')
				if(global_settings.showtabs)
					actualx += 2;
				else
					actualx += global_settings.tabstop;
			else
				actualx++;

		if(*data == '\0')
			if(--actualx < 0)
				actualx = 0;

		return actualx;
	} else
		return x;
}

void view_cursoronscreen()
{
	if(pady < padtop)
		padtop = pady;
	else if(pady >= padtop + MAX_Y - 1)
		padtop = pady - MAX_Y + 1;

	view_updatecursor();
}

static void view_updatecursor()
{
	int yactual = pady - padlim_top;

	if(yactual + MAX_Y >= padheight || yactual < padlim_top) {
		if((padlim_top = pady - MAX_Y) < 0)
			padlim_top = 0;

		yactual = pady - padlim_top;
		view_drawbuffer(buffer);
	}

	wmove(pad, yactual, view_actualx(pady, padx));

	if(padheight - padtop < MAX_Y) {
		int y = padheight - padtop;

		while(y < MAX_Y) {
			move(y++, 0);
			clrtoeol();
		}
	}

	wnoutrefresh(stdscr);
	prefresh(pad,
	         padtop, padleft,
	         0, 0,
	         MAX_Y - 1, MAX_X);
}

void view_drawbuffer(buffer_t *b)
{
	struct list *l = buffer_getindex(b, padlim_top);
	char keyword_on = 0;
	int y = 0;
	KEYWORDS;
	int size = buffer_nlines(b);

	if(size > padheight) {
		padheight = size + 16;
		resizepad();
	}

	wclear(pad);
	wmove(pad, 0, 0);

	if(!l || !l->data)
		goto tilde;

	while(l) {
		if(strchr(l->data, '\t')) {
			int len = 0;
			char *pos = l->data;

			while(len < MAX_X && *pos) {
				if(*pos == '\t')
					if(global_settings.showtabs)
						len += 2;
					else
						len += global_settings.tabstop;
				else
					len++;

				view_waddch(pad, *pos++);
			}
		} else
			waddnstr(pad, l->data, MAX_X - 1);

		waddch(pad, '\n');
		y++;
		l = l->next;
	}

tilde:
	move(y, 0);

	while(++y <= padheight)
		waddstr(pad, "~\n");
}

void view_initpad()
{
	padheight = buffer_nlines(buffer) + 16 * 2;
	padwidth = COLS;
	pad = NULL;
	resizepad();

	if(has_colors()) {
		start_color();
		use_default_colors();
		init_pair(COLOR_BLACK, -1, -1);
		init_pair(COLOR_GREEN, COLOR_GREEN, -1);
		init_pair(COLOR_WHITE, COLOR_WHITE, -1);
		init_pair(COLOR_RED, COLOR_RED, -1);
		init_pair(COLOR_CYAN, COLOR_CYAN, -1);
		init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
		init_pair(COLOR_BLUE, COLOR_BLUE, -1);
		init_pair(COLOR_YELLOW, COLOR_YELLOW, -1);
	}
}

void view_termpad()
{
	delwin(pad);
}

static void resizepad()
{
	if(pad)
		delwin(pad);

	if(padheight < buffer_nlines(buffer) + MAX_Y)
		padheight = buffer_nlines(buffer) + MAX_Y;

	if(padheight > 2048)
		padheight = 2048;

	pad = newpad(padheight, padwidth);

	if(!pad) {
		endwin();
		fprintf(stderr, "newpad(%d, %d) failed\n", padheight, padwidth);
		longjmp(allocerr, ALLOC_VIEW);
	}
}

static void clippadx()
{
	int xlim = strlen(buffer_getindex(buffer, pady)->data) - 1;

	if(xlim < 0)
		xlim = 0;

	if(padx > xlim)
		padx = xlim;
}

void view_waddch(WINDOW *w, int c)
{
	if(c == '\t') {
		if(global_settings.showtabs) {
			waddstr(w, "^I");
		} else {
			c = global_settings.tabstop;

			while(c--)
				waddch(w, ' ');
		}
	} else
		waddch(w, c);
}

void view_move(struct motion *m)
{
	struct bufferpos bp;
	struct screeninfo si;
	bp.buffer = buffer;
	bp.x = &padx;
	bp.y = &pady;
	si.padtop = padtop;
	si.padheight = MAX_Y;

	if(applymotion(m, &bp, &si))
		view_cursoronscreen();
}

enum gret gfunc(char *s, int size)
{
	/*return getnstr(s, len) == OK ? s : NULL; */
	int x, y, c, count = 0;
	enum gret r;

	/* if we're on a pad, draw over the top of it, onto stdscr */
	if(gfunc_onpad){
#ifdef USE_PAD
		getyx(pad, y, x);
#else
		getyx(stdscr, y, x);
#endif
		x -= padleft;
		y -= padtop;
		if(y > MAX_Y)
			y = MAX_Y;
		move(y, x);
		clrtoeol();
		/*wclrtoeol(pad);*/
	}else{
		/* probably a command */
		clrtoeol();

		getyx(stdscr, y, x);
	}

	do{
		(c = nc_getch());
		fprintf(stderr, "gfunc(): c: %c (%d)\n", c, c);
		switch(c){
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

#ifdef USE_PAD
					view_waddch(stdscr, c);
#else
					view_addch(c);
#endif
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
	}while(1);

exit:
	if(count < size - 1){
		s[count]   = '\n';
		s[count+1] = '\0';
	}else
		s[count] = '\0';

	if(gfunc_onpad){
#ifdef USE_PAD
		waddnstr(pad, s, MAX_X);
#else
		addnstr(s, MAX_X);
#endif
		if(s[count] != '\n')
#ifdef USE_PAD
			waddch(pad, '\n');
#else
			addch('\n');
#endif
	}else{
		addch('\n');
		move(++y, 0);
	}

	if(gfunc_onpad){
		++pady;
		padx = 0;
		/*view_cursoronscreen();*/
	}

	return r;
}

