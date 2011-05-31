#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "../range.h"
#include "../util/list.h"
#include "../buffer.h"
#include "motion.h"
#include "gui.h"
#include "marks.h"
#include "../global.h"

#define iswordpart(c) (isalnum(c) || (c) == '_')

static struct builtin_motion
builtin_motions[] = {
	[MOTION_FORWARD_LETTER]  = { 1, 0 },
	[MOTION_BACKWARD_LETTER] = { 1, 0 },

	[MOTION_FORWARD_WORD]    = { 1, 0 },
	[MOTION_BACKWARD_WORD]   = { 1, 0 },

	[MOTION_LINE_START]      = { 0, 0 },

	[MOTION_DOWN]            = { 0, 1 },
	[MOTION_UP]              = { 0, 1 },

	[MOTION_SCREEN_TOP]      = { 0, 1 },
	[MOTION_SCREEN_MIDDLE]   = { 0, 1 },
	[MOTION_SCREEN_BOTTOM]   = { 0, 1 },

	[MOTION_PARA_PREV]       = { 0, 1 },
	[MOTION_PARA_NEXT]       = { 0, 1 },

	[MOTION_PAREN_MATCH]     = { 0, 0 },

	[MOTION_ABSOLUTE_LEFT]   = { 0, 0 },
	[MOTION_ABSOLUTE_RIGHT]  = { 0, 0 },
	[MOTION_ABSOLUTE_UP]     = { 0, 1 },
	[MOTION_ABSOLUTE_DOWN]   = { 0, 1 },

	[MOTION_MARK]            = { 0, 0 },

	[MOTION_FIND]            = { 0, 0 },
	[MOTION_TIL]             = { 0, 0 }
};

int islinemotion(struct motion *m) { return builtin_motions[m->motion].is_line; }
int istilmotion( struct motion *m) { return builtin_motions[m->motion].is_til; }

/* for percent() */
static char bracketdir(char);
static char bracketrev(char);
static void percent(struct bufferpos *);


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

			case MOTION_FIND:
			case MOTION_TIL:
				m->extra = gui_getch();
				if(!isprint(m->extra)){
					gui_status(GUI_ERR, "unknown character");
					return 1;
				}
				return 0;

			case MOTION_MARK:
			{
				int c = gui_getch();
				if(mark_valid(c)){
					if(mark_isset(c)){
						m->motion = MOTION_MARK;
						m->extra = c;
					}else{
						gui_status(GUI_ERR, "mark '%c' not set", c);
						return 1;
					}
				}else{
					if(c != CTRL_AND('['))
						gui_status(GUI_ERR, "invalid mark");
					return 1;
				}
				return 0;
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
			/* TODO: ntimes */;
			if(*pos->y > 0)
				--*pos->y;
			return 0;

		case MOTION_DOWN:
		{
			/* TODO: ntimes */;
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

		case MOTION_TIL:
		case MOTION_FIND:
			charpos++; /* search _after_ the current position */
			while(*charpos && *charpos != motion->extra)
				charpos++;

			if(*charpos != '\0')
				*pos->x = charpos - charstart - (motion->motion == MOTION_TIL);
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
			/* TODO: ntimes */
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
			/* TODO: ntimes */
			if(charpos[1] != '\0')
				++*pos->x;
			return 0;

		case MOTION_BACKWARD_LETTER:
			/* TODO: ntimes */
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

			while(buffer_line_isspace(l->data))
				/* on a space, move until we find a non-space */
				NEXT();

			/* find a space */
			while(l){
				if(buffer_line_isspace(l->data))
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
			if(motion->ntimes)
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
	if(0 <= motion->ntimes &&
			motion->ntimes <= buffer_nlines(global_buffer)-1){
		*pos->y = motion->ntimes - 1;
		return 0;
	}
	return 1;
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
