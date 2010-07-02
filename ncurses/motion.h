#ifndef MOTION_H
#define MOTION_H

struct motion
{
	enum
	{
		MOTION_FORWARD_LETTER,
		MOTION_FORWARD_WORD,
		MOTION_BACKWARD_LETTER,
		MOTION_BACKWARD_WORD,


		MOTION_0, /* start of line */
		MOTION_NOSPACE, /* first non-space on a line */
		MOTION_EOL,

		MOTION_DOWN,
		MOTION_UP,

		MOTION_UNKNOWN
	} motion;

	int ntimes;
};

struct linepos
{
	struct list *line;
	char *charstart, *charpos;
};

/*
 * linepos _must_ be initialised
 * (with current pos)
 * before calling this function
 */
char/*aka bool*/ applymotion(struct motion *, struct linepos *);

#endif
