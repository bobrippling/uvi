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
#include "view.h"
#include "../util/alloc.h"

#include "../config.h"

extern struct settings global_settings;

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)
#define PAD_HEIGHT_INC 16

static void resizepad(void);
static void clippadx(void);
static void clippady(void);
static void padyinrange(void);
static int view_actualx(int, int);
static int view_getactualx(int, int);

#define clippad() do { clippady(); clippadx(); } while(0)
										/* do y first */

extern int padheight, padwidth, padtop, padleft,
						padx, pady;

extern buffer_t *buffer;
extern WINDOW *pad;

int view_scroll(enum scroll s)
{
	int ret = 0;

	switch(s){
		case SINGLE_DOWN:
			if(padtop < buffer_nlines(buffer)-1){
				padtop++;
				if(pady <= padtop){
					pady = padtop;
					clippadx();
				}
				ret = 1;
			}
			break;

		case SINGLE_UP:
			if(padtop){
				padtop--;
				if(pady >= padtop + MAX_Y){
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
			ret = buffer_nlines(buffer)-1;

			if(padtop + MAX_Y > ret)
				clear();
				/*
				 * remove the left over drawn bits
				 * now uncovered by the pad
				 */

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
			ret = buffer_nlines(buffer)-1;

			if(padtop + MAX_Y > ret)
				clear();

			if(padtop > ret)
				padtop = ret;

			padyinrange();
			ret = 1;
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

int view_move(enum direction d)
{
	int ylim, ret = 0,
			xlim, screenbottom = padtop + MAX_Y - 1;
	struct list *cl = buffer_getindex(buffer, pady);

	xlim = strlen(cl->data) - 1;
	if(xlim < 0)
		xlim = 0;

	ylim = buffer_nlines(buffer)-1;
	if(screenbottom > ylim)
		screenbottom = ylim;

	switch(d){
		case ABSOLUTE_LEFT:
			padx = 0;
			ret = 1;
			break;

		case ABSOLUTE_RIGHT:
			padx = xlim;
			ret = 1;
			break;

		case ABSOLUTE_UP:
			padtop = pady = 0;
			ret = 1;
			break;

		case ABSOLUTE_DOWN:
			pady = buffer_nlines(buffer)-1;
			if(pady >= padtop + MAX_Y){
				padtop = pady - MAX_Y + 1;
				if(padtop < 0)
					padtop = 0;
			}
			clippadx();
			ret = 1;
			break;

		case NO_BLANK:
		{
			char *c = cl->data;
			while(isspace(*c))
				c++;
			if(c == '\0'){
				if(padx){
					padx = 0;
					ret = 1;
				}
			}else if(padx != c - (char *)cl->data){
				padx = c - (char *)cl->data;
				ret = 1;
			}
			break;
		}

		case LEFT:
			if(padx > 0){
				padx--;
				ret = 1;
			}
			break;

		case RIGHT:
			if(padx < xlim){
				padx++;
				ret = 1;
			}
			break;

		case CURRENT:
			clippad();
			break;

		case UP:
		case DOWN:
			if(d == UP){
				if(pady > 0){
					pady--;
					if(padtop > pady)
						padtop = pady;
					cl = cl->prev;
				}else
					break;
			}else{
				if(pady < ylim){
					pady++;
					if(pady >= padtop + MAX_Y)
						padtop = pady - MAX_Y + 1;
					cl = cl->next;
				}else
					break;
			}

			/* only end up here has pady changed */
			clippadx();
			ret = 1;
			break;

		case SCREEN_MIDDLE:
			pady = padtop + (screenbottom - padtop) / 2;
			if(pady < 0)
				pady = 0;
			clippad();
			ret = 1;
			break;
		case SCREEN_BOTTOM:
			pady = screenbottom;
			clippad();
			ret = 1;
			break;
		case SCREEN_TOP:
			pady = padtop;
			ret = 1;
			break;
	}

	view_updatecursor();

	return ret;
}

static int view_actualx(int y, int x)
{
	/*
	 * translate the x-coord in the buffer to the screen
	 * coord, i.e. take \t into account
	 */
	struct list *l = buffer_getindex(buffer, y);
	if(l){
		char *data = l->data;
		int actualx = 0;

		while(*data && x-- > 0)
			if(*data++ == '\t')
				actualx += global_settings.tabstop;
			else
				actualx++;

		if(*data == '\0')
			if(--actualx < 0)
				actualx = 0;

		return actualx;
	}else
		return x;
}

void view_updatecursor()
{
	/* wmove takes care of pad{top,left} */
	wmove(pad, pady, view_actualx(pady, padx));

	view_refreshpad();
}

static int view_getactualx(int y, int x)
{
	char *data = buffer_getindex(buffer, y)->data;
	int actualx = 0;

	while(*data && x > 0){
		if(*data++ == '\t')
			actualx += global_settings.tabstop;
		else
			actualx++;
		--x;
	}

	return actualx + x;
}

void view_drawbuffer(buffer_t *b)
{
	struct list *l = buffer_gethead(b);
	int y = 0, size = buffer_nlines(b);

	if(size > padheight){
		padheight = size + PAD_HEIGHT_INC;
		resizepad();
	}

	wclear(pad);
	wmove(pad, 0, 0);
	if(!l || !l->data)
		goto tilde;

	while(l){
		if(strchr(l->data, '\t')){
			/* write at most MAX_X-1 chars */
			int len = 0;
			char *pos = l->data;

			while(++len < MAX_X - 2 && *pos)
				view_waddch(pad, *pos++);
		}else
			waddnstr(pad, l->data, MAX_X - 1);

		waddch(pad, '\n');
		y++;
		l = l->next;
	}

tilde:
	move(y, 0);
	while(++y <= MAX_Y)
		waddstr(pad, "~\n");

	view_updatecursor();
}

void view_refreshpad()
{
	wnoutrefresh(stdscr);
	prefresh(pad,
			padtop, padleft,   /* top left pad corner */
			0, 0,              /* top left screen (pad positioning) */
			MAX_Y - 1, MAX_X); /* bottom right of screen */
}

void view_initpad()
{
	padheight = buffer_nlines(buffer) + PAD_HEIGHT_INC * 2;
	padwidth = COLS;
	pad = NULL;

	resizepad();
}

void view_termpad()
{
	delwin(pad);
}

static void resizepad()
{
	if(pad)
		delwin(pad);

	if(padheight < MAX_Y)
		padheight = MAX_Y;

	pad = newpad(padheight, padwidth);
	if(!pad){
		endwin();
		longjmp(allocerr, 1);
	}
}

static void clippadx()
{
	/* padx, as in the position in the buffer */
	int xlim = strlen(buffer_getindex(buffer, pady)->data) - 1;
	if(xlim < 0)
		xlim = 0;

	if(padx > xlim)
		padx = xlim;
}

static void clippady()
{
	if(pady < padtop)
		padtop = pady;
	else if(pady > padtop + MAX_Y)
		padtop = pady - MAX_Y + 1;
}

/* cursor/character adding positioning code */

void view_waddch(WINDOW *w, int c)/*, int offset)*/
{
	if(c == '\t'){
		c = global_settings.tabstop;
		while(c--)
			waddch(w, ' ');
	}else
		waddch(w, c);
}

void view_putcursor(int y, int x, int offset, int tabcount)
{
	move(y - padtop, view_getactualx(y, x)
		+ offset
		+ tabcount * (global_settings.tabstop - 1)
		- padleft);
}
