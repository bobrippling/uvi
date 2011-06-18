#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>

#include "../range.h"
#include "../util/list.h"
#include "../buffer.h"
#include "motion.h"
#include "intellisense.h"
#include "gui.h"
#include "../global.h"
#include "../util/alloc.h"
#include "../util/str.h"

#define GUI_TAB_INDENT(x) \
	(global_settings.tabstop - (x) % global_settings.tabstop)


typedef struct
{
	const char *const start, *const end;
	const int colour, attrib;
} syntax;

/*#include "../config.h"*/
static void gui_position_cursor(const char *line);
static void gui_attron( enum gui_attr);
static void gui_attroff(enum gui_attr);

static int unget_i        = 0;
static int unget_buf[256] = { 0 };

static int pos_y = 0, pos_x = 0;
static int pos_top = 0, pos_left = 0;

int gui_x(){return pos_x;}
int gui_y(){return pos_y;}
int gui_max_x(){return COLS;}
int gui_max_y(){return LINES;}
int gui_top(){return pos_top;}
int gui_left(){return pos_left;}

int gui_init()
{
	static int init = 0;

	if(!init){
		init = 1;

		initscr();
		noecho();
		cbreak();
		raw(); /* use raw() to intercept ^C, ^Z */
		scrollok(stdscr, TRUE);

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
	}

	refresh();
	return 0;
}

void gui_term()
{
	endwin();
}

#define ATTR_FN(x) \
	static void gui_ ## x(enum gui_attr a) \
	{ \
		switch(a){ \
			case GUI_ERR: \
				x(COLOR_PAIR(COLOR_RED) | A_BOLD); \
				break; \
			case GUI_IS_NOT_PRINT: \
				x(COLOR_PAIR(COLOR_BLUE)); \
				break; \
			case GUI_NONE: \
				break; \
		} \
	}

ATTR_FN(attron)
ATTR_FN(attroff)

static void gui_status_trim(const char *fmt, va_list l)
{
	char buffer[256];
	int len;

	vsnprintf(buffer, sizeof buffer, fmt, l);

	if((len = strlen(buffer) - 1) >= COLS){
		int i = COLS-4;
		if(i < 0)
			i = 0;
		strcpy(buffer + i, "...");
		/* FIXME? new line + confirm */
	}

	addstr(buffer);
}

void gui_statusl(enum gui_attr a, const char *s, va_list l)
{
	int y, x;

	getyx(stdscr, y, x);

	move(LINES - 1, 0);
	gui_clrtoeol();

	gui_attron(a);
	gui_status_trim(s, l);
	gui_attroff(a);

	move(y, x);
}

void gui_status(enum gui_attr a, const char *s, ...)
{
	va_list l;
	va_start(l, s);
	gui_statusl(a, s, l); /* 14 f cali */
	va_end(l);
}

void gui_status_addl(enum gui_attr a, const char *s, va_list l)
{
	move(LINES - 1, 0);
	gui_attron(a);
	gui_status_trim(s, l);
	gui_attron(a);
	scrl(1);
}

void gui_status_add(enum gui_attr a, const char *s, ...)
{
	va_list l;
	va_start(l, s);
	gui_status_addl(a, s, l); /* aka FBI cop */
	va_end(l);
}

int gui_getch()
{
	int c;

	refresh();

	if(unget_i == 0)
		c = getch();
	else
		c = unget_buf[--unget_i];

	if(c == CTRL_AND('c'))
		raise(SIGINT);

	return c;
}

void gui_ungetch(int c)
{
	if(unget_i < (signed)(sizeof(unget_buf)/sizeof(unget_buf[0])))
		unget_buf[unget_i++] = c;
}

int gui_peekch()
{
	int c = gui_getch();
	gui_ungetch(c);
	return c;
}

void gui_clrtoeol()
{
	clrtoeol();
}

int gui_getstr(char **ps, int bspc_cancel, intellisensef intellisense)
{
	int size;
	char *start;
	int y, x, xstart;
	int i;

	if(*ps){
		free(*ps);
		*ps = NULL;
	}

	start = umalloc(size = 256);

	getyx(stdscr, y, x);

	xstart = x;

	i = 0;
	for(;;){
		int c;

		c = gui_getch();

		if(i >= size){
			size += 64;
			start = urealloc(start, size);
		}

		switch(c){
			/* TODO: ^V */

			case CTRL_AND('U'):
				x = xstart;
				i = 0;
				move(y, x);
				clrtoeol();
				break;

			case CTRL_AND('W'):
			{
				char *p;

				if(i == 0)
					break;

				p = start + i - 1;
				while(p > start && isspace(*p))
					p--;

				while(p > start && !isspace(*p))
					p--;

				x = 1 + (i = p - start);
				move(y, x);
				clrtoeol();
				break;
			}

			case CTRL_AND('?'):
			case CTRL_AND('H'):
			case 263:
			case 127:
				if(i > 0){
					char c = start[i--];

					if(isprint(c))
						move(y, --x);
					else if(c == '\t')
						if(global_settings.showtabs)
							move(y, x -= 2);
						else
							move(y, x -= global_settings.tabstop);
					else
						move(y, x -= 2);

					break;

				}else if(!bspc_cancel)
					break;
				/* else fall through */

			case CTRL_AND('['):
				start[i] = '\0';
				*ps = start;
				return 1;

			case '\n':
			case '\r':
				start[i] = '\0';
				gui_addch('\n');
				*ps = start;
				return 0;

			case CTRL_AND('N'):
				if(intellisense && !intellisense(&start, &size, &i, c)){
					/* redraw the line */
					x = xstart + strlen(start);
					mvprintw(y, xstart, "%s", start);
				}
				break;

			default:
				start[i] = c;
				x++;
				i++;
				gui_addch(c);
		}
	}
}

int gui_prompt(const char *p, char **pbuf, intellisensef is)
{
	move(LINES - 1, 0);
	gui_clrtoeol();
	addstr(p);
	return gui_getstr(pbuf, 1, is);
}


void gui_redraw()
{
	redrawwin(stdscr);
}

void gui_draw()
{
	struct list *l;
	int y;

	for(l = buffer_getindex(global_buffer, pos_top), y = 0;
			l && y < LINES - 1;
			l = l->next, y++){

		char *p;
		int i;

		move(y, 0);
		clrtoeol();

		for(p = l->data, i = 0;
				*p && i < pos_left + COLS;
				p++){

			switch(*p){
				case '\t':
					if(global_settings.showtabs)
						i += 2;
					else
						i += GUI_TAB_INDENT(i);
					break;

				default:
					if(!isprint(*p))
						i++; /* ^x */
					i++;
			}

			if(i > pos_left) /* here so we get tabs right */
				gui_addch(*p);
		}
	}

	attron( COLOR_PAIR(COLOR_BLUE) | A_BOLD);
	for(; y < LINES - 1; y++)
		mvaddstr(y, 0, "~\n");
	attroff(COLOR_PAIR(COLOR_BLUE) | A_BOLD);
	gui_position_cursor(NULL);
	refresh();
}

void gui_mvaddch(int y, int x, int c)
{
	gui_move(y, x);
	gui_addch(c);
}

void gui_addch(int c)
{
	switch(c){
		case '\t':
			if(global_settings.showtabs){
				addstr("^I");
			}else{
				int x, y;
				int ntabs;

				getyx(stdscr, y, x);
				(void)y;

				ntabs = GUI_TAB_INDENT(x);

				while(ntabs --> 0)
					addch(' ');
			}
			break;

		default:
			if(!isprint(c)){
				/*fprintf(stderr, "print unprintable: %d (%c)\n", c, c);*/
				gui_attron( GUI_IS_NOT_PRINT);
				printw("^%c", c + 'A' - 1);
				gui_attroff(GUI_IS_NOT_PRINT);
			}
			/* else fall */

		case '\n':
			addch(c);
			break;
	}
}

static void gui_position_cursor(const char *line)
{
	int x;
	int i;

	if(!line)
		line = buffer_getindex(global_buffer, pos_y)->data;

	x = 0;

	for(i = 0; i < pos_x; i++)
		if(line[i] == '\t'){
			if(global_settings.showtabs)
				x += 2;
			else
				x += GUI_TAB_INDENT(x);
		}else if(!isprint(line[i]))
			x += 2;
		else
			x++;

	move(pos_y - pos_top, x - pos_left);
}

void gui_inc_cursor()
{
	move(pos_y, pos_x + 1);
}

void gui_move(int y, int x)
{
	const char *line;
	int len;

	if(y < 0)
		y = 0;
	else if(y >= buffer_nlines(global_buffer))
		y = buffer_nlines(global_buffer)-1;

	/* check that we're on the right x pos - ^I etc */
	line = buffer_getindex(global_buffer, y)->data;

	len = strlen(line) - 1;
	if(len < 0)
		len = 0;

	if(x < 0)
		x = 0;
	else if(x > len)
		x = len;

	if(x >= pos_left + COLS)
		pos_left = x - COLS + 1;
	else if(x < pos_left)
		pos_left = x;

	if(y > pos_y)
		gui_inc(y - pos_y);
	else
		gui_dec(pos_y - y);

	pos_x = x;
	pos_y = y;
	gui_position_cursor(line);
}

void gui_inc(int n)
{
	int nl = buffer_nlines(global_buffer);

	if(pos_y < nl - 1){
		pos_y += n;

		if(pos_y >= nl)
			pos_y = nl - 1;

		if(pos_y > pos_top + LINES - 2 - SCROLL_OFF)
			pos_top = pos_y - LINES + 2 + SCROLL_OFF;
	}
}

void gui_dec(int n)
{
	if(pos_y > 0){
		pos_y -= n;
		if(pos_y < 0)
			pos_y = 0;

		if(pos_y - pos_top < SCROLL_OFF)
			pos_top = pos_y - SCROLL_OFF;
			if(pos_top < 0)
				pos_top = 0;
	}
}

void gui_move_motion(struct motion *m)
{
	struct bufferpos bp;
	struct screeninfo si;
	int x = pos_x, y = pos_y;

	bp.x      = &x;
	bp.y      = &y;
	si.top    = pos_top;
	si.height = LINES;

	if(!applymotion(m, &bp, &si))
		gui_move(y, x);
}

int gui_scroll(enum scroll s)
{
	int check = 0;
	int ret = 0;

	switch(s){
		case SINGLE_DOWN:
			if(pos_top < buffer_nlines(global_buffer) - 1){
				pos_top++;
				if(pos_y < pos_top + SCROLL_OFF)
					pos_y = pos_top + SCROLL_OFF;
				check = 1;
				ret = 1;
			}
			break;

		case SINGLE_UP:
			if(pos_top){
				pos_top--;
				check = 1;
				ret = 1;
			}
			break;

		case PAGE_UP:
			pos_top -= LINES;
			check = 1;
			ret = 1;
			break;

		case PAGE_DOWN:
			pos_top += LINES;
			check = 1;
			ret = 1;
			break;

		case HALF_UP:
			pos_top -= LINES / 2;
			check = 1;
			ret = 1;
			break;

		case HALF_DOWN:
			pos_top += LINES / 2;
			check = 1;
			ret = 1;
			break;

		case CURSOR_TOP:
			pos_top = pos_y;
			break;

		case CURSOR_BOTTOM:
			pos_top = pos_y - LINES + 2;
			break;

		case CURSOR_MIDDLE:
			pos_top = pos_y - LINES / 2;
			break;
	}

	if(pos_top < 0)
		pos_top = 0;

	if(check){
		const int lim = pos_top + LINES - 1 - SCROLL_OFF;
		if(pos_y >= lim)
			pos_y = lim - 1;
		if(pos_y < pos_top + (pos_top ? SCROLL_OFF : 0))
			pos_y = pos_top + (pos_top ? SCROLL_OFF : 0);
	}

	gui_move(pos_y, pos_x);
	return ret;
}

char *gui_current_word()
{
	return word_at(buffer_getindex(global_buffer, pos_y)->data, pos_x);
}

#if 0
void gui_drawbuffer(buffer_t *b)
{
	struct list *l = buffer_getindex(b, pos_top);
	int y = 0;

	int keyword_on = 0;
	int colour_on = 0, waitlen = 0, needcolouroff = 0;
	unsigned int current_syntax = -1;

	const char newline[] = "\n";
	KEYWORDS;
#define KEYWORD_COUNT (sizeof(keyword)/sizeof(keyword[0]))

	clear();
	move(0, 0);

	if(global_settings.colour){
		while(l){
			char *c = l->data;
			int lim = COLS - 1, i;

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
		while(l){
			if(strchr(l->data, '\t')){
				/* write at most COLS-1 chars */
				char *iter;
				int pos;

				for(iter = l->data, pos = 0;
						*iter && pos < COLS;
						iter++, pos++){

					if(*iter == '\t')
						/* FIXME: proper tabs != 8 spaces */
						if(global_settings.showtabs)
							pos += 8;
						else
							pos += global_settings.tabstop;
					else
						pos++;

					fprintf(stderr, "gui_addch('%c'), pos = %d + 1\n", *iter, pos);
					gui_addch(*iter);
				}
			}else{
				fprintf(stderr, "addnstr('%s', %d - 1);\n", l->data, COLS);
				addnstr(l->data, COLS - 1);
			}
			addch('\n');

			y++;
			l = l->next;
		}

	move(y, 0);
	if(global_settings.colour)
		coloron(COLOR_BLUE, A_BOLD);

	while(++y <= LINES)
		addstr("~\n");

	if(global_settings.colour)
		wcoloroff(COLOR_BLUE, A_BOLD);
}
#endif

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
