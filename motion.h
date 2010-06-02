#ifndef MOTION_H
#define MOTION_H

enum motion
{
	MOTION_FORWARD_LETTER,
	MOTION_FORWARD_WORD,
	MOTION_BACKWARD_LETTER,
	MOTION_BACKWARD_WORD,

	MOTION_LINE,
	MOTION_EOL,
	MOTION_UNKNOWN
} getmotion(int ch, int *repeat);

char *applymotion(enum motion, char *, int, int);

#endif
