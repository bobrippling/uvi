#include <stdarg.h>

#include "marks.h"
#include "gui.h"

struct mark
{
	int y, x;
	int set;
} marks[27] = {
	{ 0, 0, 0 }
};

static int mark_idx(int c)
{
	if(c == '\'')
		return 26;
	return c - 'a';
}

void mark_jump(void)
{
	mark_set_last(gui_y(), gui_x());
}

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

int mark_isset(int c)
{
	return marks[mark_idx(c)].set;
}

void mark_set_last(int y, int x)
{
	mark_set('\'', y, x);
}
