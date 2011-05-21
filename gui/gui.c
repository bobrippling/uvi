#include <ncurses.h>
#include <unistd.h>
#include <string.h>

#include "../range.h"
#include "../util/list.h"
#include "../buffer.h"
#include "motion.h"
#include "gui.h"
#include "../global.h"

static int max_x, max_y;

typedef struct
{
	const char *const start, *const end;
	const int colour, attrib;
} syntax;

#include "../config.h"

int gui_init()
{
	static char init = 0;

	if(!init){
		init = 1;

		initscr();
		noecho();
		cbreak();
		raw(); /* use raw() to intercept ^C, ^Z */

		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);

		if(has_colors()){
			start_color();
			use_default_colors();
			init_pair(COLOR_BLACK,   -1,            -1);
			init_pair(COLOR_GREEN,   COLOR_GREEN,   -1);
			init_pair(COLOR_WHITE,   COLOR_WHITE,   -1);
			init_pair(COLOR_RED,     COLOR_RED,     -1);
			init_pair(COLOR_CYAN,    COLOR_CYAN,    -1);
			init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
			init_pair(COLOR_BLUE,    COLOR_BLUE,    -1);
			init_pair(COLOR_YELLOW,  COLOR_YELLOW,  -1);
		}

		getmaxyx(stdscr, max_y, max_x);
	}

	refresh();
	return 0;
}

void gui_lstatus(const char *s, va_list l)
{
	move(max_y, 0);

	clrtoeol();
#ifdef VIEW_COLOUR
	if(global_settings.colour)
		coloron(COLOR_RED);
#endif
	vwprintw(stdscr, s, l);
#ifdef VIEW_COLOUR
	if(global_settings.colour)
		coloroff(COLOR_RED);
#endif
}

void gui_status(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	gui_statusl(s, l);
	va_end(l);
}

int gui_getch()
{
	int c;

	switch(read(0, &c, 1)){
		case -1:
			die("read()");
		case  0:
			return EOF;
	}

	return c;
}

void gui_refresh()
{
	struct list *l;
	int y;
	move(0, 0);

	y = 0;
	for(
			l = buffer_getindex(global_buffer, global_top);
			l && y < max_y;
			l = l->next, y++){

		addnstr((char *)l->data, max_x);
		addch('\n');
	}

	for(; y < max_y; y++)
		addstr("~\n");

	move(global_y - global_top, global_x);
	refresh();
}

void gui_addch(int c)
{
	if(c == '\t'){
		if(global_settings.showtabs){
			addstr("^I");
		}else{
			c = global_settings.tabstop;

			while(c--)
				addch(' ');
		}
	}else
		addch(c);
}

int gui_getstr(char *s, int size)
{
	return !(getnstr(s, size) == OK);
}

void gui_move(struct motion *m)
{
	struct bufferpos bp;
	struct screeninfo si;

	bp.x = &global_x;
	bp.y = &global_y;
	si.top = global_top;
	si.height = max_y;

	if(applymotion(m, &bp, &si))
		view_cursoronscreen();
}

static int clip_x()
{
	struct list *l = buffer_getindex(global_buffer, global_y);
	if(global_x > strlen(l->data))
		global_x = strlen(l->data);
}

int gui_scroll(enum scroll s)
{
	int ret = 0;

	switch(s){
		case SINGLE_DOWN:
			if(global_top < buffer_nlines(global_buffer) - 1){
				global_top++;

				if(global_y <= global_top){
					global_y = global_top;
					clipx();
				}
				ret = 1;
			}
			break;

		case SINGLE_UP:
			if(global_top){
				global_top--;

				if(global_y >= global_top + max_y){
					global_y = global_top + max_y - 1;
					clipx();
				}
				ret = 1;
			}
			break;

		case PAGE_UP:
			global_top -= max_y;
			if(global_top < 0)
				global_top = 0;

			ret = 1;
			break;

		case PAGE_DOWN:
			global_top += max_y;
			ret = buffer_nlines(global_buffer) - 1;

			if(global_top + max_y > ret)
				clear();

			if(global_top > ret)
				global_top = ret;

			ret = 1;
			break;

		case HALF_UP:
			global_top -= max_y / 2;
			if(global_top < 0)
				global_top = 0;
			ret = 1;
			break;

		case HALF_DOWN:
			global_top += max_y / 2;
			ret = buffer_nlines(global_buffer) - 1;
			if(global_top + max_y > ret)
				clear();
			if(global_top > ret)
				global_top = ret;
			ret = 1;
			break;

		case CURSOR_TOP:
			global_top = global_y;
			break;

		case CURSOR_BOTTOM:
			if((global_top = global_y - max_y + 1) < 0)
				global_top = 0;
			break;

		case CURSOR_MIDDLE:
			if((global_top = global_y - max_y / 2) < 2)
				global_top = 0;
			break;
	}

	return ret;
}

void gui_drawbuffer(buffer_t *b)
{
	struct list *l = buffer_getindex(b, global_top);
	int y = 0;
	int size = buffer_nlines(b);

#if 0
	int keyword_on = 0;
	int colour_on = 0, waitlen = 0, needcolouroff = 0;
	unsigned int current_syntax = -1;

	const char newline[] = "\n";
	KEYWORDS;
#define KEYWORD_COUNT (sizeof(keyword)/sizeof(keyword[0]))
#endif

	clear();
	move(0, 0);

#if 0
	if(global_settings.colour){
		while(l){
			char *c = l->data;
			int lim = max_x - 1, i;

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
				/* write at most max_x-1 chars */
				int len = 0;
				char *pos = l->data;

				while(len < max_x && *pos){
					if(*pos == '\t')
						if(global_settings.showtabs)
							len += 2;
						else
							len += global_settings.tabstop;
					else
						len++;

					gui_addch(*pos++);
				}
			}else
				addnstr(l->data, max_x - 1);
			addch('\n');

			y++;
			l = l->next;
		}

tilde:
	move(y, 0);
#if 0
	if(global_settings.colour)
		coloron(COLOR_BLUE, A_BOLD);
#endif

	while(++y <= max_y)
		addstr("~\n");

#if 0
	if(global_settings.colour)
		wcoloroff(COLOR_BLUE, A_BOLD);
#endif
}

#if 0
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
