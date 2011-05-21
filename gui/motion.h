#ifndef MOTION_H
#define MOTION_H

enum motiontype
{
	MOTION_FORWARD_LETTER   = 'l',
	MOTION_BACKWARD_LETTER  = 'h',

	MOTION_FORWARD_WORD     = 'w',
	MOTION_BACKWARD_WORD    = 'b',

	MOTION_LINE_START       = '^',

	MOTION_DOWN             = 'j',
	MOTION_UP               = 'k',

	MOTION_SCREEN_TOP       = 'H',
	MOTION_SCREEN_MIDDLE    = 'M',
	MOTION_SCREEN_BOTTOM    = 'L',

	MOTION_PARA_PREV        = '{',
	MOTION_PARA_NEXT        = '}',

	MOTION_PAREN_MATCH      = '%',

	MOTION_ABSOLUTE_LEFT    = '0',
	MOTION_ABSOLUTE_RIGHT   = '$',
	MOTION_ABSOLUTE_UP      = 'g',
	MOTION_ABSOLUTE_DOWN    = 'G',

	MOTION_MARK             = '\'',

	MOTION_FIND             = 'f',
	MOTION_TIL              = 't',
};

struct motion
{
	enum motiontype motion;

	int ntimes;
	char extra;
};

struct builtin_motion
{
	int is_til, is_line;
	/*
	 * is this motion a 'til' motion, i.e.
	 * should we go up to but not including
	 * the final char of the motion (when deleting)
	 */
};

struct bufferpos
{
	int *x, *y;
};

int getmotion(struct motion *);

struct screeninfo
{
	int top, height;
};

/*
 * linepos _must_ be initialised
 * (with current pos)
 * before calling this function
 */
int applymotion(struct motion *, struct bufferpos *, struct screeninfo *);

#endif
