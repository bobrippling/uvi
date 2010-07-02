#ifndef MOTION_H
#define MOTION_H

struct motion
{
	enum
	{
		MOTION_FORWARD_LETTER,
		MOTION_BACKWARD_LETTER,

		MOTION_FORWARD_WORD,
		MOTION_BACKWARD_WORD,

		MOTION_NOSPACE,        /* ^ */

		MOTION_DOWN,           /* j */
		MOTION_UP,             /* k */

		MOTION_SCREEN_TOP,     /* H */
		MOTION_SCREEN_MIDDLE,  /* M */
		MOTION_SCREEN_BOTTOM,  /* L */

		MOTION_PARA_PREV,      /* { */
		MOTION_PARA_NEXT,      /* } */

		MOTION_PAREN_MATCH,    /* % */

		MOTION_ABSOLUTE_LEFT,  /* 0 */
		MOTION_ABSOLUTE_RIGHT, /* $ */
		MOTION_ABSOLUTE_UP,    /* g */
		MOTION_ABSOLUTE_DOWN,  /* G */

		MOTION_UNKNOWN
	} motion;

	int ntimes;
};

struct linepos
{
	struct list *line;
	char *charstart, *charpos;
};


void getmotion(int (*charfunc)(void), struct motion *);

/*
 * linepos _must_ be initialised
 * (with current pos)
 * before calling this function
 */
char/*aka bool*/ applymotion(struct motion *, struct linepos *);

#endif
