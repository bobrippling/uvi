#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include "../range.h"
#include "../util/list.h"
#include "../buffer.h"
#include "motion.h"
#include "gui.h"
#include "../global.h"

typedef struct
{
	const char *const start, *const end;
	const int colour, attrib;
} syntax;

/*#include "../config.h"*/
static void gui_position_cursor(const char *line);

int pos_y = 0, pos_x = 0;
int pos_top = 0;
int max_x = 0, max_y = 0;

int gui_x(){return pos_x;}
int gui_y(){return pos_y;}
int gui_max_x(){return max_x;}
int gui_max_y(){return max_y;}
int gui_top(){return pos_top;}

static void sigwinch(int sig)
{
	(void)sig;
	getmaxyx(stdscr, max_y, max_x);
	fprintf(stderr, "sigwinch: (%d, %d)\n", max_y, max_x);
}

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

		signal(SIGWINCH, sigwinch);
		sigwinch(SIGWINCH);
	}

	refresh();
	return 0;
}

void gui_term()
{
	endwin();
}

void gui_statusl(const char *s, va_list l)
{
	int y, x;

	getyx(stdscr, y, x);

	move(max_y - 1, 0);
	gui_clrtoeol();

#ifdef VIEW_COLOUR
	if(global_settings.colour)
		coloron(COLOR_RED);
#endif
	vwprintw(stdscr, s, l);
#ifdef VIEW_COLOUR
	if(global_settings.colour)
		coloroff(COLOR_RED);
#endif
	move(y, x);
}

void gui_status(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	gui_statusl(s, l);
	va_end(l);
}

void gui_status_addl(const char *s, va_list l)
{
	move(max_y - 1, 0);
	vwprintw(stdscr, s, l);
	scrl(1);
}

void gui_status_add(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	gui_status_addl(s, l);
	va_end(l);
}

int gui_getch()
{
	int c;

	refresh();

	/* XXX: don't use read(), we need C's buffered i/o for ungetch() */
	/* FIXME: check for EINTR? */
	c = getch();

	if(c == CTRL_AND('c'))
		raise(SIGINT);

	return c;
}

int gui_peekch()
{
	int c = gui_getch();
	ungetch(c);
	return c;
}

void gui_clrtoeol()
{
	clrtoeol();
}

int gui_getstr(char *s, int size)
{
	char *const start = s;
	/*return getnstr(s, size) == OK ? 0 : 1;*/
	while(size > 0){
		int c;

		c = gui_getch();

		switch(c){
			/* TODO: ^U, ^W, etc */

			case '\b':
			case '\177':
				if(s > start){
					s--;
					size++;
					gui_addch('\b');
					break;
				}
				/* else fall through */

			case CTRL_AND('['):
				*s = '\0';
				return 1;

			case '\n':
			case '\r':
				*s = '\0';
				gui_addch('\n');
				return 0;

			default:
				*s++ = c;
				size--;
				gui_addch(c);
		}
	}
	return 0;
}

int gui_prompt(const char *p, char *buf, int siz)
{
	move(max_y - 1, 0);
	gui_clrtoeol();
	addstr(p);
	return gui_getstr(buf, siz);
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
			l && y < max_y - 1;
			l = l->next, y++){

		char *p;
		int i;

		move(y, 0);
		clrtoeol();

		for(p = l->data, i = 0;
				*p && i < max_x;
				p++){

			switch(*p){
				case '\t':
					/* FIXME: proper tabs != 8 spaces */
					if(global_settings.showtabs)
						i += 8;
					else
						i += global_settings.tabstop;
					break;

				default:
					if(!isprint(*p))
						i++; /* ^x */
					i++;
			}

			gui_addch(*p);
		}
	}

	for(; y < max_y - 1; y++)
		mvaddstr(y, 0, "~\n");

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

static void gui_position_cursor(const char *line)
{
	int x;
	int ntabs;
	int nnp;

	if(!line)
		line = buffer_getindex(global_buffer, pos_y)->data;

	for(ntabs = 0, nnp = 0, x = 0; x < pos_x; x++)
		if(line[x] == '\t')
			ntabs++;
		else if(!isprint(line[x]))
			nnp++;

	move(pos_y - pos_top,
			nnp   * 2 +
			ntabs * (global_settings.showtabs ? 2 : global_settings.tabstop) +
			x - ntabs - nnp);
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

	if(x > len)
		x = len;

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

		if(pos_y > pos_top + max_y - 2 - SCROLL_OFF)
			pos_top = pos_y - max_y + 2 + SCROLL_OFF;
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
	si.height = max_y;

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
			pos_top -= max_y;
			check = 1;
			ret = 1;
			break;

		case PAGE_DOWN:
			pos_top += max_y;
			check = 1;
			ret = 1;
			break;

		case HALF_UP:
			pos_top -= max_y / 2;
			check = 1;
			ret = 1;
			break;

		case HALF_DOWN:
			pos_top += max_y / 2;
			check = 1;
			ret = 1;
			break;

		case CURSOR_TOP:
			pos_top = pos_y;
			break;

		case CURSOR_BOTTOM:
			pos_top = pos_y - max_y + 1;
			break;

		case CURSOR_MIDDLE:
			pos_top = pos_y - max_y / 2;
			break;
	}

	if(pos_top < 0)
		pos_top = 0;

	if(check){
		const int lim = pos_top + max_y - 1 - SCROLL_OFF;
		if(pos_y >= lim)
			pos_y = lim - 1;
		if(pos_y < pos_top)
			pos_y = pos_top;
	}

	gui_move(pos_y, pos_x);
	return ret;
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
		while(l){
			if(strchr(l->data, '\t')){
				/* write at most max_x-1 chars */
				char *iter;
				int pos;

				for(iter = l->data, pos = 0;
						*iter && pos < max_x;
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
				fprintf(stderr, "addnstr('%s', %d - 1);\n", l->data, max_x);
				addnstr(l->data, max_x - 1);
			}
			addch('\n');

			y++;
			l = l->next;
		}

	move(y, 0);
	if(global_settings.colour)
		coloron(COLOR_BLUE, A_BOLD);

	while(++y <= max_y)
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
