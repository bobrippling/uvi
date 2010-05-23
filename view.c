#include <ncurses.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>

#include "list.h"
#include "buffer.h"
#include "view.h"
#include "alloc.h"

static char *strbuffer;
static int curx, cury;
extern int curline, maxy, maxx;
extern buffer_t *buffer;

void view_init()
{
	curx = cury = 0;
	strbuffer = umalloc(maxx + 1);
}

void view_term()
{
	free(strbuffer);
}

void view_move(enum direction d)
{
	struct list *cl = list_getindex(buffer_lines(buffer), curline);
	int xlim = maxx, ylim = list_count(buffer_lines(buffer))-1;

	if(cl && cl->data)
		xlim = strlen(cl->data)-1;

	switch(d){
		case LEFT:
			if(curx > 0)
				curx--;
			break;

		case RIGHT:
			if(curx < xlim)
				curx++;
			break;

		case UP:
			if(cury > 0)
				cury--;
			break;

		case DOWN:
			if(cury < ylim)
				cury++;
			break;

		case CURRENT:
			break;
	}

	curline = cury;
	move(cury, curx);
}

void view_buffer(buffer_t *b)
{
	struct list *l = list_gethead(buffer_lines(b));
	int y = 0;

	move(0, 0);
	if(!l || !l->data)
		goto tilde;

	while(l){
		strncpy(strbuffer, l->data, maxx);
		printw(strbuffer);
		addch('\n');
		y++;
		l = l->next;
	}

tilde:
	y++; /* exclude the command line */
	while(y < LINES)
		addstr("~\n"), ++y;
}
