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
	int intellisense_ch;
};

int gui_getstr(char **ps, const struct gui_read_opts *);
int gui_prompt(const char *p, char **pbuf, struct gui_read_opts *opts);
int gui_confirm(const char *p);
#endif
enum getch_opt
{
	GETCH_COOKED,      /* handle sigwinch, sigint, etc */
	GETCH_MEDIUM_RARE, /* handle sigint, etc, pass back sigwinch */
	GETCH_RAW          /* pass back all characters */
};
int gui_getch(enum getch_opt);
int gui_peekch(enum getch_opt o);
#ifdef BUFFER_H
buffer_t *gui_readfile(const char *filename);
#endif

enum gui_attr
{
	GUI_NONE         = 0,
	GUI_ERR          = 1,
	GUI_IS_NOT_PRINT = 2,

	GUI_COL_BLUE       = 0x4,
	GUI_COL_BLACK      = 0x8,
	GUI_COL_GREEN      = 0x10,
	GUI_COL_WHITE      = 0x20,
	GUI_COL_RED        = 0x40,
	GUI_COL_CYAN       = 0x80,
	GUI_COL_MAGENTA    = 0x100,
	GUI_COL_YELLOW     = 0x200,

	GUI_COL_BG_BLUE    = 0x400,
	GUI_COL_BG_BLACK   = 0x800,
	GUI_COL_BG_GREEN   = 0x1000,
	GUI_COL_BG_WHITE   = 0x2000,
	GUI_COL_BG_RED     = 0x4000,
	GUI_COL_BG_CYAN    = 0x8000,
	GUI_COL_BG_MAGENTA = 0x10000,
	GUI_COL_BG_YELLOW  = 0x20000
};

#define GUI_SEARCH_COL GUI_COL_RED

void gui_status(         enum gui_attr, const char *, ...);
void gui_statusl(        enum gui_attr, const char *, va_list);
void gui_status_add(     enum gui_attr, const char *s, ...);
void gui_status_addl(    enum gui_attr, const char *s,va_list);
void gui_status_col(     const char *, enum gui_attr, ...);
void gui_status_add_col( const char *, enum gui_attr, ...);
#define gui_status_add_start() gui_status(GUI_NONE, "")
void gui_status_wait();
void gui_show_array(enum gui_attr, int y, int x, const char **);

void gui_getyx(int *, int *);
void gui_setyx(int  , int  );
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

char *gui_current_word( void);
char *gui_current_fname(void);

int gui_macro_recording(void);
void gui_macro_record(char);
int gui_macro_complete(void);

#define CTRL_AND(c)  ((c) & 037)

#endif
