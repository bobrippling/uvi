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
#include "visual.h"
#include "motion.h"
#include "intellisense.h"
#include "gui.h"
#include "../global.h"
#include "../util/alloc.h"
#include "../str/str.h"
#include "../util/term.h"
#include "../util/io.h"
#include "macro.h"
#include "marks.h"
#include "../buffers.h"
#include "../yank.h"
#include "../util/search.h"
#include "syntax.h"

#define GUI_TAB_INDENT(x) \
	(global_settings.tabstop - (x) % global_settings.tabstop)


#if 0
typedef struct
{
	const char *const start, *const end;
	const int colour, attrib;
} syntax;
#endif

#define gui_scrolled() do{if(gui_scrollclear) gui_status(GUI_NONE, "");}while(0)

static void gui_position_cursor(const char *);
static void gui_coord_to_scr(int *py, int *px, const char *line);
static void gui_attron( enum gui_attr);
static void gui_attroff(enum gui_attr);
static void macro_append(char c);

static int   unget_i = 0, unget_size = 0;
static char *unget_buf;

static int pos_y = 0, pos_x = 0;
static int pos_top = 0, pos_left = 0;

int gui_scrollclear = 0, gui_statusrestore = 1;

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

			init_pair(9 + COLOR_BLACK,   -1, COLOR_BLACK);
			init_pair(9 + COLOR_GREEN,   -1, COLOR_GREEN);
			init_pair(9 + COLOR_WHITE,   -1, COLOR_WHITE);
			init_pair(9 + COLOR_RED,     -1, COLOR_RED);
			init_pair(9 + COLOR_CYAN,    -1, COLOR_CYAN);
			init_pair(9 + COLOR_MAGENTA, -1, COLOR_MAGENTA);
			init_pair(9 + COLOR_BLUE,    -1, COLOR_BLUE);
			init_pair(9 + COLOR_YELLOW,  -1, COLOR_YELLOW);
		}
	}

	refresh();
	return 0;
}

void gui_reload()
{
	/* put stdin into non canonical mode */
	term_canon(STDIN_FILENO, 0);
	gui_refresh();
	scrl(-1);
	gui_show_if_modified();
}

void gui_refresh()
{
	refresh();
}

void gui_term()
{
	scrl(1);
	refresh(); /* flush the scroll */
	endwin();
	term_canon(STDIN_FILENO, 1);
}

#define ATTR_FN(fn) \
	static void gui_ ## fn(enum gui_attr a) \
	{ \
		switch(a){ \
			case GUI_ERR: \
				fn(COLOR_PAIR(9 + COLOR_RED) | A_BOLD); \
				break; \
			case GUI_IS_NOT_PRINT: \
				fn(COLOR_PAIR(1 + COLOR_BLUE)); \
				break; \
			case GUI_NONE: \
				break; \
			\
			default: \
				if(a & GUI_COL_BLUE)       fn(COLOR_PAIR(1 + COLOR_BLUE)); \
				if(a & GUI_COL_BLACK)      fn(COLOR_PAIR(1 + COLOR_BLACK)); \
				if(a & GUI_COL_GREEN)      fn(COLOR_PAIR(1 + COLOR_GREEN)); \
				if(a & GUI_COL_WHITE)      fn(COLOR_PAIR(1 + COLOR_WHITE)); \
				if(a & GUI_COL_RED)        fn(COLOR_PAIR(1 + COLOR_RED)); \
				if(a & GUI_COL_CYAN)       fn(COLOR_PAIR(1 + COLOR_CYAN)); \
				if(a & GUI_COL_MAGENTA)    fn(COLOR_PAIR(1 + COLOR_MAGENTA)); \
				if(a & GUI_COL_YELLOW)     fn(COLOR_PAIR(1 + COLOR_YELLOW)); \
			\
				if(a & GUI_COL_BG_BLUE)     fn(COLOR_PAIR(9 + COLOR_BLUE)); \
				if(a & GUI_COL_BG_BLACK)    fn(COLOR_PAIR(9 + COLOR_BLACK)); \
				if(a & GUI_COL_BG_GREEN)    fn(COLOR_PAIR(9 + COLOR_GREEN)); \
				if(a & GUI_COL_BG_WHITE)    fn(COLOR_PAIR(9 + COLOR_WHITE)); \
				if(a & GUI_COL_BG_RED)      fn(COLOR_PAIR(9 + COLOR_RED)); \
				if(a & GUI_COL_BG_CYAN)     fn(COLOR_PAIR(9 + COLOR_CYAN)); \
				if(a & GUI_COL_BG_MAGENTA)  fn(COLOR_PAIR(9 + COLOR_MAGENTA)); \
				if(a & GUI_COL_BG_YELLOW)   fn(COLOR_PAIR(9 + COLOR_YELLOW)); \
		} \
	}

ATTR_FN(attron)
ATTR_FN(attroff)

static void gui_status_trim(const char *fmt, va_list l)
{
	char buffer[256];
	int len;

	len = vsnprintf(buffer, sizeof buffer, fmt, l);

	addstr(buffer);

	if(len >= COLS){
		addstr("\nnext key...");
		gui_ungetch(gui_getch(GETCH_COOKED));
		gui_status(GUI_NONE, "");
	}
}

void gui_statusl(enum gui_attr a, const char *s, va_list l)
{
	int y, x;

	if(gui_statusrestore)
		getyx(stdscr, y, x);

	move(LINES - 1, 0);
	gui_clrtoeol();

	gui_attron(a);
	gui_status_trim(s, l);
	gui_attroff(a);

	if(gui_statusrestore)
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

void gui_status_add_start()
{
	scrl(1);
}


void gui_status_wait()
{
	int y, x;
	gui_status_add(GUI_NONE, "any key to continue...");
	getyx(stdscr, y, x);
	move(y, 0);
	(void)x;
	gui_peekch(GETCH_MEDIUM_RARE);
	gui_clrtoeol();
}

void gui_show_array(enum gui_attr a, int y, int x, const char **ar)
{
	const int max_x = COLS  - x;
	const int max_y = LINES - 1;
	int i = y;

	gui_attron(a);
	while(*ar && i < max_y){
		move(i++, x - 1);
		clrtoeol();
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

int gui_getch(enum getch_opt o)
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

	if(o == GETCH_RAW)
		goto skip;

	if(c == CTRL_AND('c')){
		raise(SIGINT);
		goto restart;
	}else if(c == CTRL_AND('z')){
		gui_term();
		raise(SIGTSTP); /*raise(SIGSTOP);*/
		gui_reload();
		goto restart;
	}else if(c == 410 || c == -1){
		if(o == GETCH_MEDIUM_RARE)
			return CTRL_AND('l');
		else
			/* else we're in cooked, goto restart */
			goto restart;
	}

skip:
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

int gui_peekch(enum getch_opt o)
{
	int c = gui_getch(o);
	gui_ungetch(c);
	return c;
}

void gui_clrtoeol()
{
	clrtoeol();
}

int gui_getstr(char **ps, const struct gui_read_opts *opts)
{
#define CHECK_SIZE()                       \
		if(i >= size - 1){                     \
			size += 64;                    \
			start = urealloc(start, size); \
		}

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
	start[i] = '\0';
	for(;;){
		int c;

		c = gui_getch(GETCH_COOKED);

		CHECK_SIZE();

		switch(c){
			case CTRL_AND('U'):
				x = xstart;
				i = 0;
				*start = '\0';
				move(y, x);
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
				rnam = gui_getch(GETCH_COOKED);
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
					char c;

					c = start[--i];
					start[i] = '\0';

					if(isprint(c)){
						x--;
					}else if(c == '\t'){
						if(global_settings.showtabs){
							x -= 2;
						}else{
							x -= global_settings.tabstop;
						}
					}else{
						x -= 2;
					}

					move(y, x);

					break;
				}else if(!opts->bspc_cancel){
					break;
				}
				/* else fall through */

			case CTRL_AND('['):
				/* check for \eh or \el in rapid successession for movement */
				*ps = start;
				return 1;

			case '\n':
fin:
				if(opts->newline)
					gui_addch('\n');
				*ps = start;
				return 0;

			case CTRL_AND('v'):
			{
				int y, x;

				getyx(stdscr, y, x);

				gui_attron(GUI_COL_BLUE);
				gui_addch('^');
				gui_attroff(GUI_COL_BLUE);
				c = gui_getch(GETCH_RAW);
				move(y, x);

				goto ins_ch;
			}

			case '\t':
				if(global_settings.et && opts->allow_et){
					int i;
					for(i = 1; i < global_settings.tabstop; i++)
						gui_ungetch(' ');
					c = ' ';
				}
				/* fall */

			default:
				if(opts->intellisense && c == opts->intellisense_ch && i > 0){
					//fprintf(stderr, "calling intellisense(\"%s\")\n", start);
					if(!opts->intellisense(&start, &size, &i, c)){
						/* redraw the line */
						x = xstart + i;
						move(y, xstart);
						clrtoeol();
						addstr(start);
					}
					break;
				}
ins_ch:
				start[i++] = c;
				start[i]   = '\0';
				x++;
				CHECK_SIZE();
				gui_addch(c);
				if(opts->textw && x > opts->textw)
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

int gui_prompt(const char *p, char **pbuf, struct gui_read_opts *opts)
{
	gui_printprompt(p);

	opts->bspc_cancel = 1;

	return gui_getstr(pbuf, opts);
}

int gui_confirm(const char *p)
{
	int c;

	gui_printprompt(p);
	c = gui_getch(GETCH_COOKED);

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
	const enum visual visual = visual_get();
	int block_start, block_end;

	extern char *search_str;

	struct usearch us;
	int hls_ing, free_regex;

	struct list *l;
	int y;
	int real_y;


	real_y = pos_top;

	if(visual != VISUAL_NONE){
		visual_start = visual_get_start();
		visual_end   = visual_get_end();

		if(visual_start->end > visual_end->end){
			block_start = visual_end->end;
			block_end   = visual_start->end;
		}else{
			block_end   = visual_end->end;
			block_start = visual_start->end;
		}

		if(visual == VISUAL_LINE && visual_start->start < real_y && real_y < visual_end->start)
			attron(A_REVERSE);
	}

	free_regex = 0;
	if((hls_ing = global_settings.hls && search_str && *search_str)){
		if(usearch_init(&us, search_str))
			hls_ing = 0;
		free_regex = 1;
	}

	gui_syntax_reset();

	for(l = buffer_getindex(buffers_current(), pos_top), y = 0;
			l && y < LINES - 1;
			l = l->next, y++, real_y++){

		const char *hls;
		char *p;
		int i;

		move(y, 0);
		clrtoeol();

		if(visual == VISUAL_LINE && real_y == visual_start->start)
			attron(A_REVERSE);

		if(hls_ing)
			hls = usearch(&us, l->data, 0);
		else
			hls = NULL;


		for(p = l->data, i = 0;
				*p && i < pos_left + COLS;
				p++){

check_search:
			if(hls_ing){
				if(p == hls){
					gui_attron(GUI_SEARCH_COL);
				}else if(p == hls + usearch_matchlen(&us)){
					gui_attroff(GUI_SEARCH_COL);

					/* //g */
					hls = usearch(&us, p, 0);
					if(hls)
						goto check_search;
				}
			}

			if(visual == VISUAL_BLOCK &&
					i == block_start &&
					real_y >= visual_start->start &&
					real_y <= visual_end->start)
				attron(A_REVERSE);

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

			if(global_settings.syn)
				gui_syntax(*p, 1);
			if(i > pos_left) /* here so we get tabs right */
				gui_addch(*p);
			if(global_settings.syn)
				gui_syntax(*p, 0);

			if(visual == VISUAL_BLOCK && i > block_end)
				attroff(A_REVERSE);
		}

		gui_attroff(GUI_SEARCH_COL);

		if((visual == VISUAL_LINE && real_y == visual_end->start) || visual == VISUAL_BLOCK)
			attroff(A_REVERSE);

		if(*p && i >= COLS){
			gui_attron(GUI_CLIP_COL);
			attron(A_BOLD);
			mvaddch(y, i - pos_left - 1, '>');
			gui_attroff(GUI_CLIP_COL);
			attroff(A_BOLD);
		}
	}

	if(free_regex)
		usearch_free(&us);

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

	if(x >= pos_left + COLS - global_settings.scrolloff)
		pos_left = x - COLS + 1 + global_settings.scrolloff;
	else if(x < pos_left + global_settings.scrolloff)
		pos_left = x - global_settings.scrolloff;

	if(pos_left < 0)
		pos_left = 0;

	if(y > pos_y)
		gui_inc(y - pos_y);
	else
		gui_dec(pos_y - y);

	pos_x = x;
	pos_y = y;
	gui_position_cursor(line);
}

void gui_move_sol(int y)
{
	struct motion m;
	memset(&m, 0, sizeof m);
	gui_move(y, 0);
	m.motion = MOTION_LINE_START;
	gui_move_motion(&m);
}

void gui_inc(int n)
{
	int nl = buffer_nlines(buffers_current());

	if(pos_y < nl - 1){
		pos_y += n;

		if(pos_y >= nl)
			pos_y = nl - 1;

		if(pos_y > pos_top + LINES - 2 - global_settings.scrolloff){
			pos_top = pos_y - LINES + 2 + global_settings.scrolloff;
			gui_scrolled();
		}
	}
}

void gui_dec(int n)
{
	if(pos_y > 0){
		pos_y -= n;
		if(pos_y < 0)
			pos_y = 0;

		if(pos_y - pos_top < global_settings.scrolloff){
			pos_top = pos_y - global_settings.scrolloff;
			if(pos_top < 0)
				pos_top = 0;
			gui_scrolled();
		}
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

	if(!motion_apply(m, &bp, &si))
		gui_move(y, x);
}

int gui_scroll(enum scroll s)
{
	int check = 0;
	int ret = 0;

	switch(s){
		case SINGLE_DOWN:
			if(pos_top < buffer_nlines(buffers_current()) - 1 - global_settings.scrolloff){
				pos_top += global_settings.scrolloff;
				if(pos_y < pos_top + global_settings.scrolloff)
					pos_y = pos_top + global_settings.scrolloff;
				gui_scrolled();
				check = 1;
				ret = 1;
			}
			break;

		case SINGLE_UP:
			if(pos_top){
				pos_top -= global_settings.scrolloff;
				gui_scrolled();
				check = 1;
				ret = 1;
			}
			break;

		case PAGE_UP:
			pos_top -= LINES;
			gui_scrolled();
			check = 1;
			ret = 1;
			break;

		case PAGE_DOWN:
			pos_top += LINES;
			gui_scrolled();
			check = 1;
			ret = 1;
			break;

		case HALF_UP:
			pos_top -= LINES / 2;
			gui_scrolled();
			check = 1;
			ret = 1;
			break;

		case HALF_DOWN:
			pos_top += LINES / 2;
			gui_scrolled();
			check = 1;
			ret = 1;
			break;

		case CURSOR_TOP:
			pos_top = pos_y - global_settings.scrolloff;
			if(pos_top < 0)
				pos_top = 0;
			gui_scrolled();
			break;

		case CURSOR_BOTTOM:
			pos_top = pos_y - LINES + 2 + global_settings.scrolloff;
			gui_scrolled();
			break;

		case CURSOR_MIDDLE:
			pos_top = pos_y - LINES / 2;
			gui_scrolled();
			break;
	}

	if(pos_top < 0){
		pos_top = 0;
		gui_scrolled();
	}

	if(check){
		const int lim = pos_top + LINES - 1 - global_settings.scrolloff;
		if(pos_y >= lim)
			pos_y = lim - 1;
		if(pos_y < pos_top + (pos_top ? global_settings.scrolloff : 0))
			pos_y = pos_top + (pos_top ? global_settings.scrolloff : 0);
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
	char *ret;

	ret = gui_current(fname_at);

	if(!ret)
		return NULL;

	return str_home_replace(ret);
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
