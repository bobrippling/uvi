#include "marks.h"

struct mark
{
	int c, y, x;
} marks[26] = {
	{ 0, 0, 0 }
};

void mark_set(int c, int y, int x)
{
	marks[c - 'a'].y = y;
	marks[c - 'a'].x = x;
}

int mark_get(int c, int *y, int *x)
{
	int i = c - 'a';

	if(marks[i].y){
		*y = marks[i].y;
		*x = marks[i].x;
		return 0;
	}
	return 1;
}

int mark_isset(int c)
{
	return !!marks[c - 'a'].y;
}
