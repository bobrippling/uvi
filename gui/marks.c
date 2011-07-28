#include <stdarg.h>

#include "marks.h"
#include "gui.h"

struct mark
{
	int y, x;
	int set;
} marks[28] = {
	{ 0, 0, 0 }
};

static int mark_idx(int c)
{
	switch(c){
		case '\'': return 26;
		case '.':  return 27;
	}
	return c - 'a';
}

static void mark_cur(int c)
{
	mark_set(c, gui_y(), gui_x());
}

void mark_jump(void) { mark_cur('\''); }
void mark_edit(void) { mark_cur('.');  }

void mark_set(int c, int y, int x)
{
	int i = mark_idx(c);
	marks[i].y   = y;
	marks[i].x   = x;
	marks[i].set = 1;
}

int mark_get(int c, int *y, int *x)
{
	int i = mark_idx(c);

	if(marks[i].set){
		*y = marks[i].y;
		*x = marks[i].x;
		return 0;
	}
	return 1;
}

int mark_is_set(int c)
{
	return marks[mark_idx(c)].set;
}
