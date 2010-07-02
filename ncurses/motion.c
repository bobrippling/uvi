#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

#include "motion.h"
#define iswordpart(c) (isalnum(c) || (c) == '_')

void getmotion(int (*charfunc)(void), struct motion *m)
{
	int c;

	do
		switch((c = charfunc())){
			case 'l': m->motion = MOTION_FORWARD_LETTER;  break;
			case 'h': m->motion = MOTION_BACKWARD_LETTER; break;
			case 'w': m->motion = MOTION_FORWARD_WORD;    break;
			case 'b': m->motion = MOTION_BACKWARD_WORD;   break;

			case '0': m->motion = MOTION_0;               break;
			case '^': m->motion = MOTION_NOSPACE;         break;
			case '$': m->motion = MOTION_EOL;             break;

			case 'j': m->motion = MOTION_DOWN;            break;

			default:
				if('0' <= c && c <= '9' && repeat < INT_MAX/10){
					m->ntimes = m->ntimes * 10 + c - '0';
				}else{
					m->motion = MOTION_UNKNOWN;
					return;
				}
				break;
		}
	while(1);

	m->ntimes = repeat;
}

char applymotion(struct motion *motion, struct linepos *pos)
{
	/*
	 * basically, it works like this (for word navigation):
	 * if we're on a char, move until we're not on a char
	 * if we're not on a char, move until we're on a char
	 *
	 * and by char i mean iswordpart()
	 */
#define CURPOS (pos->charpos)

	switch(motion->motion){
		case MOTION_UNKNOWN:
			return 0;

		case MOTION_0:
			pos->charpos = pos->charstart;
			return 1;

		case MOTION_NOSPACE:
		{
			char *p = pos->charstart;
			while(isspace(*p))
				p++;
			if(*p != '\0')
				pos->charpos = p;
			else
				pos->charpos = pos->charstart;
			return 1;
		}

		case MOTION_EOL:
			while(*pos->charpos != '\0')
				pos->charpos++;
			return 1;

		case MOTION_FORWARD_WORD:
		case MOTION_BACKWARD_WORD:
		{
			const int dir = m->motion == MOTION_FORWARD_WORD ? 1 : -1;
			const char cmp = iswordpart(*CURPOS);

			while(iswordpart(*CURPOS) == cmp && *CURPOS != '\0')
				CURPOS += dir;

			if(CURPOS == '\0' && CURPOS > pos->charstart)
				CURPOS--;
			return 1;
		}

		case MOTION_FORWARD_LETTER:
			break;

		case MOTION_BACKWARD_LETTER:
			if(pos > 0)
				pos--;
			else
				goto fin;
			break;

		/* time for the line changer awkward ones */
		case MOTION_UP:
		case MOTION_DOWN:

	}

	return 0;
#undef CURPOS
}
