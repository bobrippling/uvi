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
#include "../util/term.h"
#include "../util/io.h"
#include "macro.h"
#include "marks.h"
#include "../buffers.h"
#include "visual.h"
#include "../yank.h"

#define GUI_TAB_INDENT(x) \
	(global_settings.tabstop - (x) % global_settings.tabstop)


#if 0
typedef struct
{
	const char *const start, *const end;
	const int colour, attrib;
} syntax;
#endif

static void gui_position_cursor(const char *);
static void gui_coord_to_scr(int *py, int *px, const char *line);
static void gui_attron( enum gui_attr);
static void gui_attroff(enum gui_attr);
static void macro_append(char c);

static int   unget_i = 0, unget_size = 0;
static char *unget_buf;

static int pos_y = 0, pos_x = 0;
static int pos_top = 0, pos_left = 0;

static int macro_record_char = 0;
static char *macro_str = NULL;
static int   macro_strlen = 0;

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
			init_pair(1 + COLOR_BLACK,   COLOR_BLACK,   -1);
			init_pair(1 + COLOR_GREEN,   COLOR_GREEN,   -1);
			init_pair(1 + COLOR_WHITE,   COLOR_WHITE,   -1);
			init_pair(1 + COLOR_RED,     COLOR_RED,     -1);
			init_pair(1 + COLOR_CYAN,    COLOR_CYAN,    -1);
			init_pair(1 + COLOR_MAGENTA, COLOR_MAGENTA, -1);
			init_pair(1 + COLOR_BLUE,    COLOR_BLUE,    -1);
			init_pair(1 + COLOR_YELLOW,  COLOR_YELLOW,  -1);
		}
	}

	refresh();
	return 0;
}

void gui_reload()
{
	/* put stdin into non canonical mode */
	term_canon(STDIN_FILENO, 0);
	refresh();
}

void gui_term()
{
	endwin();
	term_canon(STDIN_FILENO, 1);
}

#define ATTR_FN(fn) \
	static void gui_ ## fn(enum gui_attr a) \
	{ \
		switch(a){ \
			case GUI_ERR: \
				fn(COLOR_PAIR(1 + COLOR_RED) | A_BOLD); \
				break; \
			case GUI_IS_NOT_PRINT: \
				fn(COLOR_PAIR(1 + COLOR_BLUE)); \
				break; \
			case GUI_NONE: \
				break; \
				\
			case GUI_COL_BLUE:    fn(COLOR_PAIR(1 + COLOR_BLUE)); break; \
			case GUI_COL_BLACK:   fn(COLOR_PAIR(1 + COLOR_BLACK)); break; \
			case GUI_COL_GREEN:   fn(COLOR_PAIR(1 + COLOR_GREEN)); break; \
			case GUI_COL_WHITE:   fn(COLOR_PAIR(1 + COLOR_WHITE)); break; \
			case GUI_COL_RED:     fn(COLOR_PAIR(1 + COLOR_RED)); break; \
			case GUI_COL_CYAN:    fn(COLOR_PAIR(1 + COLOR_CYAN)); break; \
			case GUI_COL_MAGENTA: fn(COLOR_PAIR(1 + COLOR_MAGENTA)); break; \
			case GUI_COL_YELLOW:  fn(COLOR_PAIR(1 + COLOR_YELLOW)); break; \
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

void gui_status_nonl(enum gui_attr a, const char *s)
{
	gui_attron(a);
	addstr(s);
	gui_attroff(a);
}

void gui_status_addl(enum gui_attr a, const char *s, va_list l)
{
	move(LINES - 1, 0);
	gui_attron(a);
	gui_status_trim(s, l);
	gui_attroff(a);
	scrl(1);
}

void gui_status_add_col(const char *first, enum gui_attr attr, ...)
{
	va_list l;
	const char *s;

	move(LINES - 1, 0);
	gui_clrtoeol();

	gui_status_nonl(attr, first);

	va_start(l, attr);
	while((s = va_arg(l, const char *))){
		enum gui_attr a = va_arg(l, enum gui_attr);
		gui_status_nonl(a, s);
	}
	va_end(l);

	scrl(1);
}

void gui_status_add(enum gui_attr a, const char *s, ...)
{
	va_list l;
	va_start(l, s);
	gui_status_addl(a, s, l); /* aka FBI cop */
	va_end(l);
}

void gui_status_wait(int y, int x, const char *s)
{
	if(!s)
		s = "any key to continue...";
	gui_status_add(GUI_NONE, "%s\r", s);
	if(y != -1 && x != -1)
		move(y, x);
	gui_peekch();
}

void gui_show_array(enum gui_attr a, int y, int x, const char **ar)
{
	const int max_x = COLS  - x;
	const int max_y = LINES - 1;
	int i = y;

	gui_attron(a);
	while(*ar && i < max_y){
		move(i++, x - 1);
		addch(' ');
		addnstr(*ar++, max_x);
		addch(' ');
	}
	if(i < max_y){
		move(i, x - 1);
		clrtoeol();
	}
	gui_attroff(a);
	move(y, x);
}

void gui_getyx(int *y, int *x)
{
	getyx(stdscr, *y, *x);
}
void gui_setyx(int y, int x)
{
	move(y, x);
}

int gui_getch(int return_sigwinch)
{
	int c;

restart:
	refresh();

	if(unget_i == 0){
		c = getch();
		if(c == '\r')
			c = '\n';
	}else{
		c = unget_buf[--unget_i];
	}

	if(c == CTRL_AND('c')){
		raise(SIGINT);
		goto restart;
	}else if(c == CTRL_AND('z')){
		gui_term();
		raise(SIGSTOP);
		gui_reload();
		goto restart;
	}else if(c == 410 || c == -1){
		if(return_sigwinch)
			return CTRL_AND('l');
		else
			goto restart;
	}

	if(macro_record_char)
		macro_append(c);

	return c;
}

void gui_ungetch(int c)
{
	if(unget_i == unget_size)
		unget_buf = urealloc(unget_buf, unget_size += 256);

	unget_buf[unget_i++] = c;
}

void gui_queue(const char *const s)
{
	const char *p;
	for(p = s + strlen(s) - 1; p >= s; p--)
		gui_ungetch(*p);
}

int gui_peekunget()
{
	return unget_i ? unget_buf[unget_i - 1] : 0;
}

int gui_peekch()
{
	int c = gui_getch(0);
	gui_ungetch(c);
	return c;
}

void gui_clrtoeol()
{
	clrtoeol();
}

static void gui_backspace(int n)
{
	while(n --> 0)
		addch('\b');
}

int gui_getstr(char **ps, const struct gui_read_opts *opts)
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

		c = gui_getch(0);

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

			case CTRL_AND('r'):
			{
				int rnam;
				int y, x;

				getyx(stdscr, y, x);

				gui_attron(GUI_COL_BLUE);
				gui_addch('"');
				gui_attroff(GUI_COL_BLUE);
				rnam = gui_getch(0);
				move(y, x);

				if(yank_char_valid(rnam)){
					struct yank *y;

					y = yank_get(rnam);
					if(y->v && !y->is_list)
						gui_queue(y->v);
				}
				break;
			}

			case CTRL_AND('?'):
			case CTRL_AND('H'):
			case 263:
			case 127:
				if(i > 0){
					char c = start[--i];

					if(isprint(c)){
						x--;
						gui_backspace(1);
					}else if(c == '\t'){
						if(global_settings.showtabs){
							x -= 2;
							gui_backspace(2);
						}else{
							x -= global_settings.tabstop;
							gui_backspace(global_settings.tabstop);
						}
					}else{
						x -= 2;
						gui_backspace(2);
					}

					break;

				}else if(!opts->bspc_cancel)
					break;
				/* else fall through */

			case CTRL_AND('['):
				start[i] = '\0';
				*ps = start;
				return 1;

			case '\n':
fin:
				start[i] = '\0';
				if(opts->newline)
					gui_addch('\n');
				*ps = start;
				return 0;

			case CTRL_AND('N'):
			case '\t':
				if(i > 0){
					if(opts->intellisense){
						start[i] = '\0';
						if(!opts->intellisense(&start, &size, &i, c)){
							/* redraw the line */
							x = xstart + i;
							move(y, xstart);
							clrtoeol();
							addstr(start);
						}
					}
					break;
				}
				/* i == 0 --> fall through */

			default:
				start[i] = c;
				gui_addch(c);
				x++;
				if(++i > opts->textw && opts->textw)
					goto fin;
		}
	}
}

void gui_printprompt(const char *p)
{
	move(LINES - 1, 0);
	gui_clrtoeol();
	addstr(p);
}

int gui_prompt(const char *p, char **pbuf, intellisensef f)
{
	struct gui_read_opts opts;
	gui_printprompt(p);

	memset(&opts, 0, sizeof opts);
	opts.bspc_cancel  = 1;
	opts.intellisense = f;

	return gui_getstr(pbuf, &opts);
}

int gui_confirm(const char *p)
{
	int c;

	gui_printprompt(p);
	c = gui_getch(0);

	if(c == 'y' || c == 'Y')
		return 1;
	return 0;
}

void gui_redraw()
{
	redrawwin(stdscr);
}

void gui_draw()
{
	const struct range *visual_start, *visual_end;
	const int vis = visual_get() != VISUAL_NONE;

	struct list *l;
	int y;
	int real_y;

	real_y = pos_top;

	if(vis){
		visual_start = visual_get_start();
		visual_end   = visual_get_end();

		if(visual_start->start < real_y && real_y < visual_end->start)
			attron(A_REVERSE);
	}

	for(l = buffer_getindex(buffers_current(), pos_top), y = 0;
			l && y < LINES - 1;
			l = l->next, y++, real_y++){

		char *p;
		int i;

		move(y, 0);
		clrtoeol();

		if(vis && real_y == visual_start->start)
			attron(A_REVERSE);

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

		if(vis && real_y == visual_end->start)
			attroff(A_REVERSE);
	}

	attroff(A_REVERSE);

	attron( COLOR_PAIR(1 + COLOR_BLUE) | A_BOLD);
	for(; y < LINES - 1; y++)
		mvaddstr(y, 0, "~\n");
	attroff(COLOR_PAIR(1 + COLOR_BLUE) | A_BOLD);
	gui_position_cursor(NULL);
	refresh();
}

static void gui_coord_to_scr(int *py, int *px, const char *line)
{
	int y, x, max, i;

	x = 0;
	y   = *py;
	max = *px;

	if(!line){
		struct list *l = buffer_getindex(buffers_current(), y);
		if(l)
			line = l->data;
		else
			line = "";
	}

	for(i = 0; line[i] && i < max; i++)
		if(line[i] == '\t'){
			if(global_settings.showtabs)
				x += 2;
			else
				x += GUI_TAB_INDENT(x);
		}else if(!isprint(line[i])){
			x += 2;
		}else{
			x++;
		}

	*py = y - pos_top;
	*px = x - pos_left;
}

void gui_mvaddch(int y, int x, int c)
{
	gui_coord_to_scr(&y, &x, NULL);
	mvaddch(y, x, c);
}

void gui_addch(int c)
{
	switch(c){
		case '\t':
			if(global_settings.showtabs){
				gui_attron( GUI_IS_NOT_PRINT);
				addstr("^I");
				gui_attroff(GUI_IS_NOT_PRINT);
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
				if(0 <= c && c <= 'A' - 1){
					gui_attron( GUI_IS_NOT_PRINT);
					printw("^%c", c + 'A' - 1);
					gui_attroff(GUI_IS_NOT_PRINT);
					break;
				}
			}else if(c == ' ' && global_settings.list){
				gui_attron( GUI_IS_NOT_PRINT);
				addch('.');
				gui_attroff(GUI_IS_NOT_PRINT);
				break;
			}
			/* else fall */

		case '\n':
			addch(c & A_CHARTEXT);
			break;
	}
}

static void gui_position_cursor(const char *line)
{
	int x, y;

	y = pos_y;
	x = pos_x;

	gui_coord_to_scr(&y, &x, line);

	move(y, x);
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
	else if(y >= buffer_nlines(buffers_current()))
		y = buffer_nlines(buffers_current())-1;

	/* check that we're on the right x pos - ^I etc */
	line = buffer_getindex(buffers_current(), y)->data;

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
	int nl = buffer_nlines(buffers_current());

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
			if(pos_top < buffer_nlines(buffers_current()) - 1 - SCROLL_OFF){
				pos_top += SCROLL_OFF;
				if(pos_y < pos_top + SCROLL_OFF)
					pos_y = pos_top + SCROLL_OFF;
				check = 1;
				ret = 1;
			}
			break;

		case SINGLE_UP:
			if(pos_top){
				pos_top -= SCROLL_OFF;
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
			pos_top = pos_y - SCROLL_OFF;
			if(pos_top < 0)
				pos_top = 0;
			break;

		case CURSOR_BOTTOM:
			pos_top = pos_y - LINES + 2 + SCROLL_OFF;
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

char *gui_current(char *(*f)(const char *, int))
{
	return f(buffer_getindex(buffers_current(), pos_y)->data, pos_x);
}

char *gui_current_word()
{
	return gui_current(word_at);
}

char *gui_current_fname()
{
	const char *home;
	char *ret;

	ret = gui_current(fname_at);

	if(!ret)
		return NULL;

	home = getenv("HOME");
	if(!home)
		home = "/";
	/* TODO: ~luser */
	return str_expand(ret, '~', home);
}

int gui_macro_recording()
{
	return !!macro_record_char;
}

void gui_macro_record(char c)
{
	macro_record_char = c;
}

int gui_macro_complete()
{
	const int c = macro_record_char;
	macro_set(macro_record_char, macro_str);
	macro_str = NULL;
	macro_strlen = 0;
	macro_record_char = 0;
	return c;
}

static void macro_append(char c)
{
	char s[2];

	s[0] = c;
	s[1] = '\0';

	ustrcat(&macro_str, &macro_strlen, s, NULL);
}
