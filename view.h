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

	CURRENT
};

void	view_drawbuffer(buffer_t *);
int		view_move(enum direction);
void	view_refreshpad(WINDOW *);

#endif
