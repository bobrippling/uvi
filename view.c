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

#include "list.h"
#include "range.h"
#include "buffer.h"
#include "view.h"
#include "alloc.h"

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)
#define PAD_HEIGHT_INC 16

static void resizepad(void);

extern int padheight, padwidth, padtop, padleft, padx, pady;
extern buffer_t *buffer;
extern WINDOW *pad;

int view_scroll(enum scroll s)
{
	switch(s){
		case SINGLE_DOWN:
			if(padtop < buffer_nlines(buffer)-1){
				padtop++;
				if(pady < padtop)
					pady = padtop;
				return 1;
			}
			break;

		case SINGLE_UP:
			if(padtop){
				padtop--;
				if(pady > padtop + padheight)
					pady = padtop + padheight;
				return 1;
			}
			break;
	}
	return 0;
}

int view_move(enum direction d)
{
	struct list *cl = buffer_getindex(buffer, pady);
	int xlim = MAX_X, ylim = buffer_nlines(buffer)-1, ret = 0;

	if(cl && cl->data)
		xlim = strlen(cl->data)-1;

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
			pady = 0;
			ret = 1;
			break;

		case ABSOLUTE_DOWN:
			pady = buffer_nlines(buffer)-1;
			ret = 1;
			break;

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
			break;

		case UP:
		case DOWN:
			if(d == UP){
				if(pady > 0){
					pady--;
					cl = cl->prev;
				}else
					break;
			}else{
				if(pady < ylim){
					pady++;
					cl = cl->next;
				}else
					break;
			}

			/* only end up here has pady changed */
			if(cl){
				xlim = strlen(cl->data)-1;
				if(xlim < 0)
					xlim = 0;
				if(padx > xlim)
					padx = xlim;
				ret = 1;
			}
			break;
	}

	if(pady > MAX_Y)
		padtop = pady - LINES;

	view_updatecursor();

	return ret;
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
		char *tab;
		if((tab = strchr(l->data, '\t'))){
			int len = 0;
			char *pos = l->data;

			do{
				int i = tab - pos;
				len += i;

				waddnstr(pad, pos, i);
				waddch(pad, ' '); /* Tab replacement char here */

				pos = tab + 1;
				tab = strchr(pos, '\t');
			}while(len < MAX_X && tab);

			if(*pos)
				waddnstr(pad, pos, MAX_X - len);
		}else
			waddnstr(pad, l->data, MAX_X);

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

void view_refreshpad(WINDOW *p)
{
	wnoutrefresh(stdscr);
	prefresh(p,
			padtop, padleft,   /* top left pad corner */
			0, 0,              /* top left screen (pad positioning) */
			MAX_Y - 1, MAX_X); /* bottom right of screen */
	view_updatecursor();
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
