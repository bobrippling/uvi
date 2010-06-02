#include <stdlib.h>
#include <ctype.h>

#include "motion.h"
#define iswordpart(c) (isalnum(c) || (c) == '_')

enum motion getmotion(int ch, int linechar)
{
	switch(ch){
		case 'l':
			return MOTION_FORWARD_LETTER;
		case 'h':
			return MOTION_BACKWARD_LETTER;
		case 'w':
			return MOTION_FORWARD_WORD;
		case 'b':
			return MOTION_BACKWARD_WORD;

		default:
			if(ch == linechar)
				return MOTION_LINE;
			return MOTION_UNKNOWN;
	}
}

char *applymotion(enum motion m, char *s, int pos, int repeat)
{
	/*
	 * basically, it works like this (for word navigation):
	 * if we're on a char, move until we're not on a char
	 * if we're not on a char, move until we're on a char
	 *
	 * and by char i mean iswordpart()
	 */
	const char cmp = iswordpart(s[pos]);

	if(!s[pos])
		return s + pos;

	do{
		switch(m){
			case MOTION_FORWARD_WORD:
				while(iswordpart(s[pos]) == cmp)
					pos++;
				break;

			case MOTION_BACKWARD_WORD:
				while(pos > 0 && iswordpart(s[pos]) == cmp)
					pos--;
				break;

			case MOTION_FORWARD_LETTER:
				pos++;
				break;
			case MOTION_BACKWARD_LETTER:
				pos--;
				break;

			case MOTION_EOL:
				while(s[pos] != '\0')
					pos++;
				repeat = 0;
				break;

			case MOTION_LINE:
			case MOTION_UNKNOWN:
				return NULL;
		}
	}while(--repeat >= 0);

	return s + pos;
}
