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
	MOTION_FIND_REV         = 'F',
	MOTION_TIL_REV          = 'T',
	MOTION_FIND_NEXT        = ';',
	MOTION_FIND_PREV        = ',',

	MOTION_NOMOVE           =  0 /* used for dd, cc and x */
};

struct motion
{
	enum motiontype motion;

	int ntimes;
	char extra;
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

int islinemotion(struct motion *m);
int istilmotion( struct motion *m);
int isbigmotion( struct motion *m);

const char *motion_str(struct motion *);

#endif
