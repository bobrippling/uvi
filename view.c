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

extern int maxy, maxx, curx, cury;
extern buffer_t *buffer;


void view_move(enum direction d)
{
	struct list *cl = buffer_getindex(buffer, cury);
	int xlim = maxx, ylim = buffer_nlines(buffer)-1;

	if(cl && cl->data)
		xlim = strlen(cl->data)-1;

	switch(d){
		case ABSOLUTE_LEFT:
			curx = 0;
			break;

		case ABSOLUTE_RIGHT:
			curx = xlim;
			break;

		case LEFT:
			if(curx > 0)
				curx--;
			break;

		case RIGHT:
			if(curx < xlim)
				curx++;
			break;

		case CURRENT:
			break;

		case UP:
		case DOWN:
			if(d == UP){
				if(cury > 0){
					cury--;
					cl = cl->prev;
				}else
					break;
			}else{
				if(cury < ylim){
					cury++;
					cl = cl->next;
				}else
					break;
			}

			/* only end up here has cury changed */
			if(cl){
				xlim = strlen(cl->data)-1;
				if(xlim < 0)
					xlim = 0;
				if(curx > xlim)
					curx = xlim;
			}
			break;
	}

	move(cury, curx);
}

void view_buffer(buffer_t *b)
{
	struct list *l = buffer_gethead(b);
	int y = 0;

	move(0, 0);
	if(!l || !l->data)
		goto tilde;

	while(l){
		addnstr(l->data, maxx);
		addch('\n');
		y++;
		l = l->next;
	}

tilde:
	y++; /* exclude the command line */
	while(y < LINES)
		addstr("~\n"), ++y;
}
