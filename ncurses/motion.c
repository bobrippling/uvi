#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

#include "motion.h"
#define iswordpart(c) (isalnum(c) || (c) == '_')

enum motion getmotion(int (*gfunc)(void), int *repeat)
{
	int c;
	do
		switch((c = gfunc())){
			case 'l':
				return MOTION_FORWARD_LETTER;
			case 'h':
				return MOTION_BACKWARD_LETTER;
			case 'w':
				return MOTION_FORWARD_WORD;
			case 'b':
				return MOTION_BACKWARD_WORD;
			case '$':
				return MOTION_EOL;
			case 'c':
			case 'd':
				return MOTION_LINE;

			default:
				if('0' <= c && c <= '9' && *repeat < INT_MAX/10)
					*repeat = *repeat * 10 + c - '0';
				else
					return MOTION_UNKNOWN;
		}
	while(1);
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
	do{
		const char cmp = iswordpart(s[pos]);

		switch(m){
			case MOTION_FORWARD_WORD:
				while(iswordpart(s[pos]) == cmp && s[pos] != '\0')
					pos++;
				if(s[pos] == '\0'){
					pos--;
					goto fin;
				}
				break;

			case MOTION_BACKWARD_WORD:
				while(pos > 0 && iswordpart(s[pos]) == cmp && pos > 0)
					pos--;
				if(pos == 0)
					goto fin;
				break;

			case MOTION_FORWARD_LETTER:
				if(s[pos] == '\0')
					goto fin;
				else if(s[++pos] == '\0'){
					pos--;
					goto fin;
				}
				break;

			case MOTION_BACKWARD_LETTER:
				if(pos > 0)
					pos--;
				else
					goto fin;
				break;

			case MOTION_EOL:
				while(s[pos] != '\0')
					pos++;
				pos--;
				goto fin;

			case MOTION_LINE:
			case MOTION_UNKNOWN:
				return NULL;
		}
	}while(--repeat > 0);

fin:
	return s + pos;
}
