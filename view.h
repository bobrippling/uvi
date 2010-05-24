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

void view_buffer(buffer_t *);
void view_init(void);
void view_term(void);
void view_move(enum direction);

#endif
