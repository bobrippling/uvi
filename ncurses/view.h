#ifndef NC_PRINT_H
#define NC_PRINT_H

enum direction
{
	LEFT,
	RIGHT,
	UP,
	DOWN,

	ABSOLUTE_LEFT,
	ABSOLUTE_RIGHT,
	ABSOLUTE_UP,
	ABSOLUTE_DOWN,

	NO_BLANK,

	SCREEN_TOP,
	SCREEN_MIDDLE,
	SCREEN_BOTTOM,

	PARA_NEXT,
	PARA_PREV,

	CURRENT
};

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

void	view_initpad(void);
void	view_termpad(void);
void	view_drawbuffer(buffer_t *);

int		view_move(enum direction);

int		view_scroll(enum scroll);
void	view_refreshpad(void);

void	view_updatecursor(void);

void	view_waddch(WINDOW *, int);
void	view_putcursor(int, int, int, int);

int	view_getactualx(int, int);

#endif
