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

		MOTION_NO_MOVE,        /* i.e. for deleting the current char */

		MOTION_NOSPACE,        /* ^ */

		MOTION_DOWN,           /* j */
		MOTION_UP,             /* k */
		MOTION_LINE,           /* no key */

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

		MOTION_MARK,           /* ' */

		MOTION_UNKNOWN
	} motion;

	int ntimes;
	char extra;
};

struct bufferpos
{
	buffer_t *buffer;
	int *x, *y;
};

char islinemotion(struct motion *);
char istilmotion(struct motion *);

void getmotion(void status(const char *, ...),
		int (*charfunc)(void), struct motion *);

struct screeninfo
{
	int padtop, padheight;
};

/*
 * linepos _must_ be initialised
 * (with current pos)
 * before calling this function
 */
char/*aka bool*/ applymotion(struct motion *,
		struct bufferpos *, struct screeninfo *);

#endif
