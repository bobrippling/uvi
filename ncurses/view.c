/*
 * view.c handles displaying the buffer,
 * as opposed to ncurses.c, which handles
 * user i/o and commands
 */
#include <ncurses.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <ctype.h>

#include "../util/list.h"
#include "../range.h"
#include "../buffer.h"
#include "view.h"
#include "../util/alloc.h"

#include "../config.h"

extern struct settings global_settings;

#define MAX_X (COLS - 1)
#define MAX_Y (LINES - 1)
#define PAD_HEIGHT_INC 16

static void resizepad(void);
static void padyinrange(void);
static int view_actualx(int, int);
static void clippadx(void);
static void clippady(void);
#define clippad() do { clippady(); clippadx(); } while(0)
/* do y first */


#if VIEW_COLOUR
# define wcoloron( c, a)  (wattron( pad, COLOR_PAIR(c) | (a) ))
# define wcoloroff(c, a)  (wattroff(pad, COLOR_PAIR(c) | (a) ))
#endif

extern int padheight, padwidth, padtop, padleft, padx, pady;

extern buffer_t *buffer;
extern WINDOW *pad;

int view_scroll(enum scroll s)
{
	int ret = 0;

	switch(s){
		case SINGLE_DOWN:
			if(padtop < buffer_nlines(buffer)-1){
				padtop++;
				if(pady <= padtop){
					pady = padtop;
					clippadx();
				}
				ret = 1;
			}
			break;

		case SINGLE_UP:
			if(padtop){
				padtop--;
				if(pady >= padtop + MAX_Y){
					pady = padtop + MAX_Y - 1;
					clippadx();
				}
				ret = 1;
			}
			break;

		case PAGE_UP:
			padtop -= MAX_Y;
			if(padtop < 0)
				padtop = 0;
			padyinrange();
			ret = 1;
			break;

		case PAGE_DOWN:
			padtop += MAX_Y;
			ret = buffer_nlines(buffer)-1;

			if(padtop + MAX_Y > ret)
				clear();
				/*
				 * remove the left over drawn bits
				 * now uncovered by the pad
				 */

			if(padtop > ret)
				padtop = ret;

			padyinrange();
			ret = 1;
			break;

		case HALF_UP:
			padtop -= MAX_Y / 2;
			if(padtop < 0)
				padtop = 0;
			padyinrange();
			ret = 1;
			break;

		case HALF_DOWN:
			padtop += MAX_Y / 2;
			ret = buffer_nlines(buffer)-1;

			if(padtop + MAX_Y > ret)
				clear();

			if(padtop > ret)
				padtop = ret;

			padyinrange();
			ret = 1;
			break;
	}

	view_updatecursor();
	return ret;
}

static void padyinrange()
{
	if(pady < padtop)
		pady = padtop;
	else if(pady > padtop + MAX_Y)
		pady = padtop + MAX_Y - 1;
	clippadx();
}

int view_move(enum direction d)
{
	int ylim, ret = 0,
			xlim, screenbottom = padtop + MAX_Y - 1;
	struct list *cl = buffer_getindex(buffer, pady);

	xlim = strlen(cl->data) - 1;
	if(xlim < 0)
		xlim = 0;

	ylim = buffer_nlines(buffer)-1;
	if(screenbottom > ylim)
		screenbottom = ylim;

	switch(d){
		case LEFT:
			if(padx > 0){
				padx--;
				ret = 1;
			}
			break;

		case RIGHT:
			if(padx < xlim){
				padx++;
				ret = 1;
			}
			break;

		case UP:
		case DOWN:
			if(d == UP){
				if(pady > 0){
					pady--;
					if(padtop > pady)
						padtop = pady;
					cl = cl->prev;
				}else
					break;
			}else{
				if(pady < ylim){
					pady++;
					if(pady >= padtop + MAX_Y)
						padtop = pady - MAX_Y + 1;
					cl = cl->next;
				}else
					break;
			}

			/* only end up here has pady changed */
			clippadx();
			ret = 1;
			break;

		case PARA_PREV:
			ret = 1;
		case PARA_NEXT:
		{
			struct list *l = buffer_getindex(buffer, pady);
			int diff = 0;

			if(!strlen(l->data)){
				if(ret)
					while(l->prev){
						l = l->prev;
						diff--;
						if(strlen(l->data))
							break;
					}
				else
					while(l->next){
						l = l->next;
						diff++;
						if(strlen(l->data))
							break;
					}
			}

			if(ret){
				while(l->prev){
					l = l->prev;
					diff--;
					if(!strlen(l->data)){
						pady += diff;
						clippad();
						break;
					}
				}
			}else{
				while(l->next){
					l = l->next;
					diff++;
					if(!strlen(l->data)){
						pady += diff;
						clippad();
						break;
					}
				}
			}

			ret = 0;
			break;
		}

		case ABSOLUTE_LEFT:
			padx = 0;
			ret = 1;
			break;

		case ABSOLUTE_RIGHT:
			padx = xlim;
			ret = 1;
			break;

		case ABSOLUTE_UP:
			padtop = pady = 0;
			ret = 1;
			break;

		case ABSOLUTE_DOWN:
			pady = buffer_nlines(buffer)-1;
			if(pady >= padtop + MAX_Y){
				padtop = pady - MAX_Y + 1;
				if(padtop < 0)
					padtop = 0;
			}
			clippadx();
			ret = 1;
			break;

		case NO_BLANK:
		{
			char *c = cl->data;
			while(isspace(*c))
				c++;
			if(c == '\0'){
				if(padx){
					padx = 0;
					ret = 1;
				}
			}else if(padx != c - (char *)cl->data){
				padx = c - (char *)cl->data;
				ret = 1;
			}
			break;
		}

		case CURRENT:
			clippad();
			break;

		case SCREEN_MIDDLE:
			pady = padtop + (screenbottom - padtop) / 2;
			if(pady < 0)
				pady = 0;
			clippad();
			ret = 1;
			break;
		case SCREEN_BOTTOM:
			pady = screenbottom;
			clippad();
			ret = 1;
			break;
		case SCREEN_TOP:
			pady = padtop;
			ret = 1;
			break;
	}

	view_updatecursor();

	return ret;
}

int view_getactualx(int y, int x)
{
	/*
	 * gets the x, no matter where on screen
	 */
	struct list *pos = buffer_getindex(buffer, y);
	char *data;
	int actualx = 0;

	if(!pos)
		return x;

	data = pos->data;
	while(*data && x > 0){
		if(*data++ == '\t')
			if(global_settings.showtabs)
				actualx += 2;
			else
				actualx += global_settings.tabstop;
		else
			actualx++;
		--x;
	}

	return actualx + x;
}

static int view_actualx(int y, int x)
{
	/*
	 * translate the x-coord in the buffer to the screen
	 * coord, i.e. take \t into account
	 *
	 * this also clips it to the buffer
	 */
	struct list *l = buffer_getindex(buffer, y);
	if(l){
		char *data = l->data;
		int actualx = 0;

		while(*data && x --> 0)
			if(*data++ == '\t')
				if(global_settings.showtabs)
					actualx += 2;
				else
					actualx += global_settings.tabstop;
			else
				actualx++;

		if(*data == '\0')
			if(--actualx < 0)
				actualx = 0;

		return actualx;
	}else
		return x;
}

void view_updatecursor()
{
	/* wmove takes care of pad{top,left} */
	wmove(pad, pady, view_actualx(pady, padx));

	view_refreshpad();
}

#if VIEW_COLOUR
static void checkcolour(const char *, char *const,
		char *constconst, unsigned int *, char *);

static void checkcolour(const char *c, char *waitlen, char *colour_on,
		unsigned int *current, char *needcolouroff)
{
	unsigned int i;
	SYNTAXES;
#define SYNTAX_COUNT  (sizeof(syntax)/sizeof(syntax[0]))

	if(*waitlen <= 0){
		if(*colour_on){
			/* assert(*current >= 0); */
			const char *ptr = syntax[i = *current].end;

			if(!strncmp(c, ptr, syntax[i].elen)){
				*colour_on = 0;
				if(c[1] == '\0'){
					wcoloroff(syntax[i].colour, syntax[i].attrib);
				}else{
					int stayon = 0;
					unsigned int j;

					c++;
					for(j = 0; j < SYNTAX_COUNT; j++)
						if(!strncmp(c, syntax[j].start, syntax[j].slen)){
							stayon = 1;
							wcoloroff(syntax[i].colour, syntax[i].attrib);
							wcoloron( syntax[j].colour, syntax[j].attrib);
							break;
						}

					if(!stayon){
						*needcolouroff = 1;
						*waitlen = syntax[i].elen;
					}
					/*
					 * wait until the chars have been added before removing colour
					 * below in the else bit
					 */
				}
			}
		}else
			for(i = 0; i < SYNTAX_COUNT; i++){
				const char *ptr = *colour_on ? syntax[i].end : syntax[i].start;

				if(!strncmp(c, ptr, *colour_on ? syntax[i].elen : syntax[i].slen)){
					wcoloron(syntax[i].colour, syntax[i].attrib);

					*current = i;
					*colour_on = 1;
					*waitlen = syntax[i].slen - 1;
					break;
				}
			}
	}else if(--*waitlen == 0){
		if(*needcolouroff){
			wcoloroff(syntax[*current].colour, syntax[*current].attrib);
			*needcolouroff = 0;
		}
	}
}
#endif

void view_drawbuffer(buffer_t *b)
{
	struct list *l = buffer_gethead(b);
	char keyword_on = 0;
	int y = 0, size = buffer_nlines(b);
#if VIEW_COLOUR
	char colour_on = 0, waitlen = 0, needcolouroff = 0;
	unsigned int current_syntax = -1;
	const char newline[] = "\n";
#endif
	KEYWORDS;
#define KEYWORD_COUNT (sizeof(keyword)/sizeof(keyword[0]))

	if(size > padheight){
		padheight = size + PAD_HEIGHT_INC;
		resizepad();
	}

	wclear(pad);
	wmove(pad, 0, 0);
	if(!l || !l->data)
		goto tilde;

#if VIEW_COLOUR
	wattrset(pad, A_NORMAL); /* bit of a bodge */
	if(global_settings.colour){
		while(l){
			char *c = l->data;
			int lim = MAX_X - 1, i;

			while(*c && lim > 0){
				checkcolour(c, &waitlen, &colour_on,
						&current_syntax, &needcolouroff);

				if(keyword_on && !--keyword_on)
					/* a keyword's last character has been added */
					wcoloroff(KEYWORD_COLOUR, A_BOLD);

				if(*c == '\t')
					if(global_settings.showtabs)
						lim -= 2;
					else
						lim -= global_settings.tabstop;
				else
					lim--;

				if(!colour_on && !keyword_on)
					for(i = 0; i < (signed)KEYWORD_COUNT; i++)
						if(!strncmp(keyword[i].kw, c, keyword[i].len)){
							wcoloron(KEYWORD_COLOUR, A_BOLD);
							keyword_on = keyword[i].len;
							break;
						}

				view_waddch(pad, *c++);
			}

			while(*c)
				checkcolour(c++, &waitlen, &colour_on,
						&current_syntax, &needcolouroff);

			checkcolour(newline, &waitlen, &colour_on,
					&current_syntax, &needcolouroff);

			waddch(pad, '\n');
			y++;
			l = l->next;
		}
	}else
#endif
	while(l){
		if(strchr(l->data, '\t')){
			/* write at most MAX_X-1 chars */
			int len = 0;
			char *pos = l->data;

			while(len < MAX_X && *pos){
				if(*pos == '\t')
					if(global_settings.showtabs)
						len += 2;
					else
						len += global_settings.tabstop;
				else
					len++;

				view_waddch(pad, *pos++);
			}
		}else
			waddnstr(pad, l->data, MAX_X - 1);

		waddch(pad, '\n');
		y++;
		l = l->next;
	}

tilde:
	move(y, 0);
#if VIEW_COLOUR
	if(global_settings.colour)
		wcoloron(COLOR_BLUE, 0);
#endif
	while(++y <= MAX_Y)
		waddstr(pad, "~\n");
#if VIEW_COLOUR
	if(global_settings.colour)
		wcoloroff(COLOR_BLUE, 0);
#endif

	view_updatecursor();
}

void view_refreshpad()
{
	wnoutrefresh(stdscr);
	prefresh(pad,
			padtop, padleft,   /* top left pad corner */
			0, 0,              /* top left screen (pad positioning) */
			MAX_Y - 1, MAX_X); /* bottom right of screen */
}

void view_initpad()
{
	padheight = buffer_nlines(buffer) + PAD_HEIGHT_INC * 2;
	padwidth = COLS;
	pad = NULL;

	resizepad();

	if(has_colors()){
		start_color();
		use_default_colors();
		/* tells (n)curses that -1 means the default colour, so it can be used below in init_pair() */
		/* init_color(COLOR_RED, 700, 0, 0); RGB are out of 1000                  */
		init_pair(COLOR_BLACK,   -1,            -1);
		init_pair(COLOR_GREEN,   COLOR_GREEN,   -1);
		init_pair(COLOR_WHITE,   COLOR_WHITE,   -1);
		init_pair(COLOR_RED,     COLOR_RED,     -1);
		init_pair(COLOR_CYAN,    COLOR_CYAN,    -1);
		init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
		init_pair(COLOR_BLUE,    COLOR_BLUE,    -1);
		init_pair(COLOR_YELLOW,  COLOR_YELLOW,  -1);
	}
}

void view_termpad()
{
	delwin(pad);
}

static void resizepad()
{
	if(pad)
		delwin(pad);

	if(padheight < MAX_Y)
		padheight = MAX_Y;

	pad = newpad(padheight, padwidth);
	if(!pad){
		endwin();
		longjmp(allocerr, ALLOC_VIEW);
	}
}

static void clippadx()
{
	/* padx, as in the position in the buffer */
	int xlim = strlen(buffer_getindex(buffer, pady)->data) - 1;
	if(xlim < 0)
		xlim = 0;

	if(padx > xlim)
		padx = xlim;
}

static void clippady()
{
	if(pady < padtop)
		padtop = pady;
	else if(pady > padtop + MAX_Y - 1)
		padtop = pady - MAX_Y + 1;
}

/* cursor/character adding positioning code */

void view_waddch(WINDOW *w, int c)/*, int offset)*/
{
	if(c == '\t'){
		if(global_settings.showtabs){
			waddstr(w, "^I");
		}else{
			c = global_settings.tabstop;
			while(c--)
				waddch(w, ' ');
		}
	}else
		waddch(w, c);
}

void view_putcursor(int y, int x, int offset, int tabcount)
{
	move(y - padtop, view_getactualx(y, x)
		+ offset
		+ tabcount * (global_settings.tabstop - 1)
		- padleft);
}
