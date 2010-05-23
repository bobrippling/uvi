#include <ncurses.h>
#include <math.h>

#include "list.h"
#include "buffer.h"
#include "view.h"


void viewbuffer(buffer_t *buffer, int cl)
{
	struct list *l = list_getindex(buffer_lines(buffer), cl);
	int y = 0;

	if(!l || !l->data)
		return;

	move(0, 0);
	while(l){
		printw(l->data);
		addch('\n');
		y++;
		l = l->next;
	}

	y++; /* exclude the command line */
	while(y < LINES)
		addstr("~\n"), ++y;
}
