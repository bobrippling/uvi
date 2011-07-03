#ifndef GUI_H
#define GUI_H

int  gui_init(void);
void gui_reload(void);
void gui_term(void);
void gui_run(void);

int gui_x(void);
int gui_y(void);
int gui_max_x(void);
int gui_max_y(void);
int gui_top(void);
int gui_left(void);

void gui_addch(int c);
void gui_ungetch(int c);
int  gui_peekunget(void);
void gui_queue(const char *s);
void gui_mvaddch(int y, int x, int c);

#ifdef INTELLISENSE_H
struct gui_read_opts
{
	int bspc_cancel;
	int textw;
	int newline;
	intellisensef intellisense;
};

int gui_getstr(char **ps, const struct gui_read_opts *);
int gui_prompt(const char *p, char **, intellisensef);
#endif
int gui_getch(void);
int gui_peekch(void);

enum gui_attr
{
	GUI_NONE,
	GUI_ERR,
	GUI_IS_NOT_PRINT,

	GUI_COL_BLUE,
	GUI_COL_BLACK,
	GUI_COL_GREEN,
	GUI_COL_WHITE,
	GUI_COL_RED,
	GUI_COL_CYAN,
	GUI_COL_MAGENTA,
	GUI_COL_YELLOW,
};

void gui_status(         enum gui_attr, const char *, ...);
void gui_statusl(        enum gui_attr, const char *, va_list);
void gui_status_add(     enum gui_attr, const char *s, ...);
void gui_status_addl(    enum gui_attr, const char *s,va_list);
void gui_status_col(     const char *, enum gui_attr, ...);
void gui_status_add_col( const char *, enum gui_attr, ...);
void gui_status_wait(void);

void gui_clrtoeol(void);

enum scroll
{
	SINGLE_DOWN,
	SINGLE_UP,

	HALF_DOWN,
	HALF_UP,

	PAGE_DOWN,
	PAGE_UP,

	CURSOR_MIDDLE,
	CURSOR_TOP,
	CURSOR_BOTTOM
};

#ifdef BUFFER_H
void gui_drawbuffer(buffer_t *b);
#endif

int  gui_scroll(enum scroll);
void gui_inc(int n);
void gui_dec(int n);
void gui_move(int y, int x);
void gui_inc_cursor(void);
#ifdef MOTION_H
void gui_move_motion(struct motion *);
#endif

void gui_clip(void);
void gui_draw(void);
void gui_redraw(void);

char *gui_current_word(void);

int gui_macro_recording(void);
void gui_macro_record(char);
int gui_macro_complete(void);

#define CTRL_AND(c)  ((c) & 037)
#define SCROLL_OFF 3

#endif
