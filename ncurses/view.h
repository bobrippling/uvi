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

#ifdef USE_PAD
void view_initpad(void);
void view_termpad(void);

void view_waddch(WINDOW *, int c);
#else
void view_addch(int c);
#endif

void view_drawbuffer(buffer_t *b);

int  view_scroll(enum scroll);
void view_move(struct motion *);
int  view_getactualx(int y, int x);

void view_cursoronscreen(void);

#endif
