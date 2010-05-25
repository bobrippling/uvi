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

	CURRENT
};

void	view_drawbuffer(buffer_t *);
int		view_move(enum direction);
void	view_refreshpad(WINDOW *);

#define view_updatecursor() wmove(pad, pady, padx)

#endif
