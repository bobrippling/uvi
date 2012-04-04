#include <stdarg.h>

#include "marks.h"
#include "gui.h"

struct mark
{
	int y, x;
	int set;
} marks[NMARKS] = {
	{ 0, 0, 0 }
};

/*
 * 0-25: 'a' - 'z'
 * 26-35: '0' - '9'
 * 36: last
 * 37: '.'
 */

static int mark_idx(int c)
{
	if(!mark_valid(c))
		return 0;

	switch(c){
		case '\'': return 36;
		case '.':  return 37;
	}
	if('a' <= c && c <= 'z')
		return c - 'a' + 10;
	return c - '0';
}

char mark_from_idx(int c)
{
	switch(c){
		case 36: return '\'';
		case 37: return '.';
	}
	if(c >= 10)
		return c - 10 + 'a';
	return c + '0';

}

static void mark_cur(int c)
{
	mark_set(c, gui_y(), gui_x());
}

int mark_count()
{
	return NMARKS;
}

void mark_jump(void) { mark_cur('\''); }
void mark_edit(void) { mark_cur('.');  }

void mark_set_idx(int i, int y, int x)
{
	marks[i].y   = y;
	marks[i].x   = x;
	marks[i].set = 1;
}

void mark_set(int c, int y, int x)
{
	mark_set_idx(mark_idx(c), y, x);
}

int mark_get_idx(int i, int *y, int *x)
{
	if(marks[i].set){
		*y = marks[i].y;
		*x = marks[i].x;
		return 0;
	}
	return 1;
}

int mark_get(int c, int *y, int *x)
{
	return mark_get_idx(mark_idx(c), y, x);
}

int mark_is_set(int c)
{
	return marks[mark_idx(c)].set;
}

int mark_find(int y, int x)
{
	int i;
	for(i = 0; i < NMARKS; i++)
		if(marks[i].set && marks[i].y == y && marks[i].x == x)
			return mark_from_idx(i);
	return -1;
}
