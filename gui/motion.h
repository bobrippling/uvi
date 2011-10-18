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

	MOTION_FUNC_PREV        = '[',
	MOTION_FUNC_NEXT        = ']',

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

	/* not user-enterable */
	MOTION_WHOLE_LINE       = 1, /* used for dd, cc */
	MOTION_NOMOVE,               /* used for x */
};

struct motion
#ifdef VISUAL_H
{
	enum motiontype motion;
	enum visual     visual_state;

	int ntimes;
	char extra;
}
#endif
;

struct bufferpos
{
	int *x, *y;
};

int motion_get(struct motion *m, int allow_visual, int multiple,
		const char *ifthese, enum motiontype tothis);

struct screeninfo
{
	int top, height;
};

/*
 * linepos _must_ be initialised
 * (with current pos)
 * before calling this function
 */
int motion_apply(const struct motion *, struct bufferpos *, struct screeninfo *);

int motion_is_line(struct motion *m);
int motion_is_til( struct motion *m);
int motion_is_big( struct motion *m);

int motion_wrap(int *x, int *y, int *x2, int *y2, const char *ifthese, enum motiontype tothis);

const char *motion_str(struct motion *);

#endif
