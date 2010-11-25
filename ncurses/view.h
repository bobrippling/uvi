#ifndef NC_VIEW_H
#define NC_VIEW_H

enum scroll
{
	SINGLE_DOWN,
	SINGLE_UP,

	HALF_DOWN,
	HALF_UP,

	PAGE_DOWN,
	PAGE_UP,

	CURSOR_MIDDLE,
	CURSOR_TOP,
	CURSOR_BOTTOM
};

void view_initpad(void);
void view_termpad(void);

int view_scroll(enum scroll);
void view_move(struct motion *);

void view_cursoronscreen(void);

#endif
