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
void view_drawbuffer(buffer_t *);

int  view_scroll(enum scroll);

void view_cursoronscreen(void);

void view_waddch(WINDOW *, int);
void view_putcursor(int, int, int, int);

int view_getactualx(int, int);

void view_move(struct motion *);

#endif
