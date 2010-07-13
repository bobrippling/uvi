#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

#include "../util/list.h"
#include "../range.h"
#include "../buffer.h"
#include "motion.h"
#include "marks.h"

#define iswordpart(c) (isalnum(c) || (c) == '_')


/* for percent() */
static char bracketdir(char);
static char bracketrev(char);
static void percent(struct bufferpos *);

char islinemotion(struct motion *m)
{
	switch(m->motion){
		case MOTION_FORWARD_LETTER:
		case MOTION_BACKWARD_LETTER:
		case MOTION_FORWARD_WORD:
		case MOTION_BACKWARD_WORD:
		case MOTION_NOSPACE:
		case MOTION_PAREN_MATCH:
		case MOTION_ABSOLUTE_LEFT:
		case MOTION_ABSOLUTE_RIGHT:
		case MOTION_MARK:
		case MOTION_NO_MOVE:
			return 0;

		case MOTION_DOWN:
		case MOTION_UP:
		case MOTION_SCREEN_TOP:
		case MOTION_SCREEN_MIDDLE:
		case MOTION_SCREEN_BOTTOM:
		case MOTION_PARA_PREV:
		case MOTION_PARA_NEXT:
		case MOTION_ABSOLUTE_UP:
		case MOTION_ABSOLUTE_DOWN:
		case MOTION_LINE:
			return 1;

		case MOTION_UNKNOWN:
			break;
	}
	return 0;
}

void getmotion(void status(const char *, ...),
		int (*charfunc)(void), struct motion *m)
{
	int c;

/*
 * s/^ *\([^,]*\), *\/\* \(.\).*$/\t\t\tcase '\2': m->motion = \1; return;/
 */

	/* m->ntimes is already initialised */

	do
		switch((c = charfunc())){
			case 'l': m->motion = MOTION_FORWARD_LETTER;  return;
			case 'h': m->motion = MOTION_BACKWARD_LETTER; return;
			case 'w': m->motion = MOTION_FORWARD_WORD;    return;
			case 'b': m->motion = MOTION_BACKWARD_WORD;   return;

			case '^': m->motion = MOTION_NOSPACE;         return;

			case 'j': m->motion = MOTION_DOWN;            return;
			case 'k': m->motion = MOTION_UP;              return;

			case 'H': m->motion = MOTION_SCREEN_TOP;      return;
			case 'M': m->motion = MOTION_SCREEN_MIDDLE;   return;
			case 'L': m->motion = MOTION_SCREEN_BOTTOM;   return;

			case '{': m->motion = MOTION_PARA_PREV;       return;
			case '}': m->motion = MOTION_PARA_NEXT;       return;

			case '%': m->motion = MOTION_PAREN_MATCH;     return;

			case '0': m->motion = MOTION_ABSOLUTE_LEFT;   return;
			case '$': m->motion = MOTION_ABSOLUTE_RIGHT;  return;

			case 'g': m->motion = MOTION_ABSOLUTE_UP;     return;
			case 'G': m->motion = MOTION_ABSOLUTE_DOWN;   return;

			case '\'':
				c = charfunc();
				if(validmark(c)){
					if(mark_isset(c)){
						m->motion = MOTION_MARK;
						m->extra = c;
					}else{
						status("mark not set");
						m->motion = MOTION_UNKNOWN;
					}
				}else{
					status("invalid mark");
					m->motion = MOTION_UNKNOWN;
				}
				return;

			default:
				if('0' <= c && c <= '9' && m->ntimes < INT_MAX/10){
					m->ntimes = m->ntimes * 10 + c - '0';
				}else{
					m->motion = MOTION_UNKNOWN;
					return;
				}
		}
	while(1);
}

char applymotion(struct motion *motion, struct bufferpos *pos,
		struct screeninfo *si)
{
	/*
	 * basically, it works like this (for word navigation):
	 * if we're on a char, move until we're not on a char
	 * if we're not on a char, move until we're on a char
	 *
	 * and by char i mean iswordpart()
	 */
	char *const charstart = buffer_getindex(pos->buffer, *pos->y)->data;
	char *      charpos   = charstart + *pos->x;

#define CLIPX() \
	do{ \
		int len = strlen(buffer_getindex(pos->buffer, *pos->y)->data) - 1; \
\
		if(len < 0) \
			len = 0; \
\
		if(*pos->x > len) \
			*pos->x = len; \
	}while(0)

	switch(motion->motion){
		case MOTION_UNKNOWN:
			return 0;

		case MOTION_NO_MOVE:
			return 1;

		case MOTION_LINE:
		{
			int n = motion->ntimes - 1;
			if(n < 0)
				n = 0;

			*pos->y += n;
			return 1;
		}

		case MOTION_MARK:
			if(mark_get(motion->extra, pos->y, pos->x)){
				/* buffer may have changed since mark was set... */
				if(*pos->y >= buffer_nlines(pos->buffer))
					*pos->y = buffer_nlines(pos->buffer)-1;
				return 1;
			}
			return 0;

		case MOTION_ABSOLUTE_LEFT:
			*pos->x = 0;
			return 1;

		case MOTION_NOSPACE:
		{
			charpos = charstart;
			while(isspace(*charpos))
				charpos++;
			if(*charpos != '\0')
				*pos->x = charpos - charstart;
			else
				*pos->x = 0;
			return 1;
		}

		case MOTION_ABSOLUTE_RIGHT:
			while(*charpos != '\0')
				charpos++;
			if(charpos > charstart)
				charpos--;
			*pos->x = charpos - charstart;
			return 1;

		case MOTION_FORWARD_WORD:
		case MOTION_BACKWARD_WORD:
		{
			/* TODO: ntimes */
			const int dir = motion->motion == MOTION_FORWARD_WORD ? 1 : -1;
			const char cmp = iswordpart(*charpos);

			while(iswordpart(*charpos) == cmp && *charpos != '\0')
				charpos += dir;

			if(charpos == '\0' && charpos > charstart)
				charpos--;

			*pos->x = charpos - charstart;
			return 1;
		}

		case MOTION_FORWARD_LETTER:
			/* TODO: ntimes */
			if(charpos[1] != '\0'){
				++*pos->x;
				return 1;
			}
			break;

		case MOTION_BACKWARD_LETTER:
			/* TODO: ntimes */
			if(*pos->x > 0)
				--*pos->x;
			return 1;

		/* time for the line changer awkward ones */
		case MOTION_UP:
			/* TODO: ntimes */;
			if(*pos->y > 0){
				--*pos->y;
				CLIPX();
				return 1;
			}
			break;

		case MOTION_DOWN:
		{
			int nlines = buffer_nlines(pos->buffer) - 1;
			/* TODO: ntimes */;

			if(nlines < 0)
				nlines = 0;

			if(*pos->y < nlines){
				++*pos->y;
				CLIPX();
				return 1;
			}
			break;
		}

		case MOTION_SCREEN_TOP:
			if(*pos->y != si->padtop){
				*pos->y = si->padtop;
				CLIPX();
				return 1;
			}
			break;

		case MOTION_SCREEN_MIDDLE:
		{
			int mid;

			if(buffer_nlines(pos->buffer) - si->padtop > si->padheight)
				mid = si->padtop + si->padheight/2;
			else{
				mid = si->padtop + (buffer_nlines(pos->buffer) - si->padtop)/2;
				if(mid < 0)
					mid = 0;
			}

			if(*pos->y != mid){
				*pos->y = mid;

				CLIPX();
				return 1;
			}
			break;
		}

		case MOTION_SCREEN_BOTTOM:
			if(*pos->y != si->padtop + si->padheight - 1){
				*pos->y = si->padtop + si->padheight - 1;
				if(*pos->y >= buffer_nlines(pos->buffer))
					*pos->y = buffer_nlines(pos->buffer)-1;
				CLIPX();
				return 1;
			}
			break;

		case MOTION_PARA_PREV:
			/* FIXME TODO: ntimes */;
			return 0;

		case MOTION_PARA_NEXT:
			/* FIXME TODO: ntimes */;
			return 0;

		case MOTION_PAREN_MATCH:
			percent(pos);
			return 1;

		case MOTION_ABSOLUTE_UP:
			if(motion->ntimes)
				goto MOTION_GOTO;
			*pos->y = 0;
			*pos->x = 0;
			return 1;

		case MOTION_ABSOLUTE_DOWN:
		{
			int last;
			if(motion->ntimes)
				goto MOTION_GOTO;

			last = buffer_nlines(pos->buffer) - 1;
			if(last < 0)
				last = 0;

			if(*pos->y != last){
				*pos->y = last;
				CLIPX();
				return 1;
			}
			break;
		}
	}

	return 0;
MOTION_GOTO:
	if(0 <= motion->ntimes &&
			motion->ntimes <= buffer_nlines(pos->buffer)-1){
		*pos->y = motion->ntimes - 1;
		CLIPX();
		return 1;
	}
	return 0;
}

static void percent(struct bufferpos *lp)
{
	struct list *listpos = buffer_getindex(lp->buffer, *lp->y);
	char *line = listpos->data,
			*pos = line + *lp->x, *opposite, dir,
			bracket = '\0', revbracket = '\0', xychanged = 0;
	int nest = 0, pady_save = *lp->y, padx_save = *lp->x;

#define restorepadxy() do{ \
												*lp->x = padx_save; \
												*lp->y = pady_save; \
												/*status("matching bracket not found");*/ \
												}while(0)

	while(pos >= line && !bracket)
		if((revbracket = bracketrev(*pos))){
			bracket = *pos;
			break;
		}else
			pos--;

	if(!revbracket)
		return;

	dir = bracketdir(revbracket);

	/* looking for revbracket */
	opposite = pos + dir;

	do{
		while(opposite >= line && *opposite){
			if(*opposite == revbracket){
				if(--nest < 0)
					break;
			}else if(*opposite == bracket)
				nest++;
			opposite += dir;
		}

		if(opposite < line){
			if(listpos->prev){
				listpos = listpos->prev;
				--*lp->y;
				xychanged = 1;

				line = listpos->data;
				opposite = line + strlen(line) - 1;
			}else{
				restorepadxy();
				return;
			}
		}else if(!*opposite){
			if(listpos->next){
				listpos = listpos->next;
				++*lp->y;
				xychanged = 1;

				opposite = line = listpos->data;
			}else{
				restorepadxy();
				return;
			}
		}else
			break;
	}while(1);

	if(opposite)
		*lp->x = opposite - line;
#undef restorepadxy
}

static char bracketdir(char b)
{
	switch(b){
		case '<':
		case '(':
		case '[':
		case '{':
			return -1;

		case '>':
		case ')':
		case ']':
		case '}':
			return 1;

		default:
			return 0;
	}
}

static char bracketrev(char b)
{
	switch(b){
		case '<': return '>';
		case '(': return ')';
		case '[': return ']';
		case '{': return '}';

		case '>': return '<';
		case ')': return '(';
		case ']': return '[';
		case '}': return '{';

		default: return '\0';
	}
}
