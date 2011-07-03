#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../range.h"
#include "../util/list.h"
#include "../buffer.h"
#include "motion.h"
#include "intellisense.h"
#include "gui.h"
#include "marks.h"
#include "../global.h"
#include "../util/str.h"

#define iswordpart(c) (isalnum(c) || (c) == '_')

static struct builtin_motion
{
	int is_til, is_line, is_big;
	/*
	 * is this motion a 'til' motion, i.e.
	 * should we go up to but not including
	 * the final char of the motion (when deleting)
	 *
	 * is_big - do we set '-mark?
	 */

	int is_ntimes;
} builtin_motions[] = {
	[MOTION_FORWARD_LETTER]  = { 1, 0, 0, 1 },
	[MOTION_BACKWARD_LETTER] = { 1, 0, 0, 1 },

	[MOTION_FORWARD_WORD]    = { 1, 0, 0, 1 },
	[MOTION_BACKWARD_WORD]   = { 1, 0, 0, 1 },

	[MOTION_LINE_START]      = { 0, 0, 0, 0 },

	[MOTION_DOWN]            = { 0, 1, 0, 1 },
	[MOTION_UP]              = { 0, 1, 0, 1 },

	[MOTION_SCREEN_TOP]      = { 0, 1, 1, 0 },
	[MOTION_SCREEN_MIDDLE]   = { 0, 1, 1, 0 },
	[MOTION_SCREEN_BOTTOM]   = { 0, 1, 1, 0 },

	[MOTION_PARA_PREV]       = { 0, 1, 1, 1 },
	[MOTION_PARA_NEXT]       = { 0, 1, 1, 1 },

	[MOTION_PAREN_MATCH]     = { 0, 0, 0, 1 },

	[MOTION_ABSOLUTE_LEFT]   = { 0, 0, 0, 0 },
	[MOTION_ABSOLUTE_RIGHT]  = { 0, 0, 0, 0 },
	[MOTION_ABSOLUTE_UP]     = { 0, 1, 1, 0 },
	[MOTION_ABSOLUTE_DOWN]   = { 0, 1, 1, 0 },

	[MOTION_MARK]            = { 0, 0, 1, 0 },

	[MOTION_FIND]            = { 0, 0, 1, 1 },
	[MOTION_TIL]             = { 1, 0, 1, 1 },
	[MOTION_FIND_REV]        = { 0, 0, 1, 1 },
	[MOTION_TIL_REV]         = { 1, 0, 1, 1 },
	[MOTION_FIND_NEXT]       = { 0, 0, 1, 1 },

	[MOTION_NOMOVE]          = { 0, 1, 0, 0 },
};

static int last_find_c = 0, last_find_til = 0, last_find_rev = 0;

int isbigmotion( struct motion *m) { return builtin_motions[m->motion].is_big; }
int islinemotion(struct motion *m) { return builtin_motions[m->motion].is_line; }
int istilmotion( struct motion *m)
{
	return m->motion == MOTION_FIND_NEXT ?
		last_find_til :
		builtin_motions[m->motion].is_til;
}

/* for percent() */
static char bracketdir(char);
static char bracketrev(char);
static void percent(struct bufferpos *);

static int applymotion2(
		struct motion *motion,
		struct bufferpos *pos,
		struct screeninfo *si);


int getmotion(struct motion *m)
{
	/* m->ntimes is already initialised */

	do{
		switch((m->motion = gui_getch())){
			case MOTION_FORWARD_LETTER:
			case MOTION_BACKWARD_LETTER:
			case MOTION_FORWARD_WORD:
			case MOTION_BACKWARD_WORD:
			case MOTION_LINE_START:
			case MOTION_DOWN:
			case MOTION_UP:
			case MOTION_SCREEN_TOP:
			case MOTION_SCREEN_MIDDLE:
			case MOTION_SCREEN_BOTTOM:
			case MOTION_PARA_PREV:
			case MOTION_PARA_NEXT:
			case MOTION_PAREN_MATCH:
			case MOTION_ABSOLUTE_LEFT:
			case MOTION_ABSOLUTE_RIGHT:
			case MOTION_ABSOLUTE_UP:
			case MOTION_ABSOLUTE_DOWN:
				return 0;

			case MOTION_FIND_PREV:
			case MOTION_FIND_NEXT:
				m->extra = last_find_c;
				if(last_find_rev ^ (m->motion == MOTION_FIND_PREV))
					if(last_find_til)
						m->motion = MOTION_TIL_REV;
					else
						m->motion = MOTION_FIND_REV;
				else
					if(last_find_til)
						m->motion = MOTION_TIL;
					else
						m->motion = MOTION_FIND;
				return 0;

			case MOTION_FIND:
			case MOTION_TIL:
			case MOTION_FIND_REV:
			case MOTION_TIL_REV:
				m->extra = gui_getch();
				if(!isprint(m->extra) && m->extra != '\t'){
					gui_status(GUI_ERR, "unknown find character");
					return 1;
				}
				last_find_c = m->extra;
				last_find_til = m->motion == MOTION_TIL      || m->motion == MOTION_TIL_REV;
				last_find_rev = m->motion == MOTION_FIND_REV || m->motion == MOTION_TIL_REV;
				return 0;

			case MOTION_MARK:
			{
				int c = gui_getch();
				if(mark_valid(c)){
					if(mark_isset(c)){
						m->motion = MOTION_MARK;
						m->extra = c;
						return 0;
					}else{
						gui_status(GUI_ERR, "mark '%c' not set", c);
						return 1;
					}
				}else{
					if(c != CTRL_AND('['))
						gui_status(GUI_ERR, "invalid mark");
					return 1;
				}
				/* unreachable */
			}

			default:
				if('0' <= m->motion && m->motion <= '9' && m->ntimes < INT_MAX/10)
					m->ntimes = m->ntimes * 10 + m->motion - '0';
				else
					return 1;
		}
	}while(1);
}

int applymotion(struct motion *motion, struct bufferpos *pos,
		struct screeninfo *si)
{
	if(motion->ntimes == 0)
		motion->ntimes = 1;

	do
		if(applymotion2(motion, pos, si))
			return 1;
		else if(!builtin_motions[motion->motion].is_ntimes)
			return 0;
	while(motion->ntimes --> 1);

	return 0;
}

int applymotion2(struct motion *motion, struct bufferpos *pos,
		struct screeninfo *si)
{
	/*
	 * basically, it works like this (for word navigation):
	 * if we're on a char, move until we're not on a char
	 * if we're not on a char, move until we're on a char
	 *
	 * and by char i mean iswordpart()
	 */
	char *const charstart = buffer_getindex(global_buffer, *pos->y)->data;
	char *      charpos   = charstart + *pos->x;

	switch(motion->motion){
		case MOTION_UP:
			if(*pos->y > 0)
				--*pos->y;
			return 0;

		case MOTION_DOWN:
		{
			int nlines = buffer_nlines(global_buffer);

			if(nlines < 0)
				nlines = 0;

			if(*pos->y < nlines - 1){
				++*pos->y;
				return 0;
			}
			break;
		}

		case MOTION_NOMOVE:
			return 0;

		case MOTION_MARK:
			return mark_get(motion->extra, pos->y, pos->x);

		case MOTION_ABSOLUTE_LEFT:
			*pos->x = 0;
			return 0;

		case MOTION_LINE_START:
			charpos = charstart;
			while(isspace(*charpos))
				charpos++;
			if(*charpos != '\0')
				*pos->x = charpos - charstart;
			else
				*pos->x = 0;
			return 0;

		case MOTION_FIND_NEXT: /* should never get this */
		case MOTION_FIND_PREV: /* or this */
		case MOTION_TIL:
		case MOTION_FIND:
			charpos++; /* search _after_ the current position */
			while(*charpos && *charpos != motion->extra)
				charpos++;
			if(*charpos != '\0')
				*pos->x = charpos - charstart - (motion->motion == MOTION_TIL);
			return 0;

		case MOTION_TIL_REV:
		case MOTION_FIND_REV:
			charpos--;
			while(charpos >= charstart && *charpos != motion->extra)
				charpos--;
			if(charpos >= charstart && *charpos == motion->extra)
				*pos->x = charpos - charstart + (motion->motion == MOTION_TIL);
			return 0;


		case MOTION_ABSOLUTE_RIGHT:
			while(*charpos != '\0')
				charpos++;
			if(charpos > charstart)
				charpos--;
			*pos->x = charpos - charstart;
			return 0;

		case MOTION_FORWARD_WORD:
		case MOTION_BACKWARD_WORD:
		{
			const int cmp = iswordpart(*charpos);

			if(motion->motion == MOTION_FORWARD_WORD){
				while(*charpos && iswordpart(*charpos) == cmp)
					charpos++;
				if(!*charpos && charpos > charstart)
					charpos--;
			}else{
				while(charpos > charstart && iswordpart(*charpos) == cmp)
					charpos--;
			}

			*pos->x = charpos - charstart;
			return 0;
		}

		case MOTION_FORWARD_LETTER:
			if(charpos[1] != '\0')
				++*pos->x;
			return 0;

		case MOTION_BACKWARD_LETTER:
			if(*pos->x > 0)
				--*pos->x;
			return 0;

		/* time for the line changer awkward ones */
		case MOTION_SCREEN_TOP:
			*pos->y = si->top ? si->top + SCROLL_OFF : 0;
			return 0;

		case MOTION_SCREEN_MIDDLE:
		{
			int mid;

			if(buffer_nlines(global_buffer) - si->top > si->height)
				mid = si->top + si->height/2;
			else{
				mid = si->top + (buffer_nlines(global_buffer) - si->top - 1)/2;
				if(mid < 0)
					mid = 0;
			}

			*pos->y = mid;
			return 0;
		}

		case MOTION_SCREEN_BOTTOM:
			*pos->y = si->top + si->height - 2 - SCROLL_OFF;

			if(*pos->y >= buffer_nlines(global_buffer))
				*pos->y = buffer_nlines(global_buffer)-1;
			return 0;

		case MOTION_PARA_PREV:
		case MOTION_PARA_NEXT:
		{
			const int rev = motion->motion == MOTION_PARA_PREV;
			int y = *pos->y;
			struct list *l = buffer_getindex(global_buffer, y);
#define NEXT() \
			do \
				if(rev){ \
					l = l->prev; \
					y--; \
				}else{ \
					l = l->next; \
					y++; \
				} \
			while(0)

			while(l && line_isspace(l->data))
				/* on a space, move until we find a non-space */
				NEXT();

			/* find a space */
			while(l){
				if(line_isspace(l->data))
					break;
				NEXT();
			}

			if(l)
				*pos->y = y;
			else
				*pos->y = rev ? 0 : buffer_nlines(global_buffer);

			*pos->x = 0;
			return 0;
#undef NEXT
		}

		case MOTION_PAREN_MATCH:
			percent(pos);
			return 0;

		case MOTION_ABSOLUTE_UP:
			if(motion->ntimes)
				goto MOTION_GOTO;
			*pos->y = 0;
			*pos->x = 0;
			return 0;

		case MOTION_ABSOLUTE_DOWN:
		{
			int last;
			if(motion->ntimes > 1)
				goto MOTION_GOTO;

			last = buffer_nlines(global_buffer) - 1;
			if(last < 0)
				last = 0;

			*pos->y = last;
			return 0;
		}
	}

	return 1;
MOTION_GOTO:
	{
		int y = motion->ntimes - 1;
		if(y < 0)
			y = 0;
		else if(y >= buffer_nlines(global_buffer))
			y = buffer_nlines(global_buffer) - 1;

		*pos->y = y;
	}
	return 0;
}

static void percent(struct bufferpos *lp)
{
	struct list *listpos = buffer_getindex(global_buffer, *lp->y);
	char *line = listpos->data;
	char *pos = line + *lp->x, *opposite;
	int dir;
	char bracket = '\0', revbracket = '\0';
	int nest = 0;

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

				line = listpos->data;
				opposite = line + strlen(line) - 1;
			}else{
				return;
			}
		}else if(!*opposite){
			if(listpos->next){
				listpos = listpos->next;
				++*lp->y;

				opposite = line = listpos->data;
			}else{
				return;
			}
		}else
			break;
	}while(1);

	if(opposite)
		*lp->x = opposite - line;
#undef restorexy
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

const char *motion_str(struct motion *m)
{
#define S(x) case x: return #x
	switch(m->motion){
		S(MOTION_FORWARD_LETTER);
		S(MOTION_BACKWARD_LETTER);
		S(MOTION_FORWARD_WORD);
		S(MOTION_BACKWARD_WORD);
		S(MOTION_LINE_START);
		S(MOTION_DOWN);
		S(MOTION_UP);
		S(MOTION_SCREEN_TOP);
		S(MOTION_SCREEN_MIDDLE);
		S(MOTION_SCREEN_BOTTOM);
		S(MOTION_PARA_PREV);
		S(MOTION_PARA_NEXT);
		S(MOTION_PAREN_MATCH);
		S(MOTION_ABSOLUTE_LEFT);
		S(MOTION_ABSOLUTE_RIGHT);
		S(MOTION_ABSOLUTE_UP);
		S(MOTION_ABSOLUTE_DOWN);
		S(MOTION_MARK);
		S(MOTION_FIND);
		S(MOTION_TIL);
		S(MOTION_FIND_REV);
		S(MOTION_TIL_REV);
		S(MOTION_FIND_NEXT);
		S(MOTION_FIND_PREV);
		S(MOTION_NOMOVE);
	}
	return NULL;
}
