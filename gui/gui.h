#ifndef GUI_H
#define GUI_H

int  gui_init(void);
void gui_term(void);
int  gui_main(const char *filename, int readonly);

int gui_x(void);
int gui_y(void);
int gui_max_x(void);
int gui_max_y(void);
int gui_top(void);

void gui_addch(int c);
void gui_mvaddch(int y, int x, int c);

int gui_getstr(char *s, int size);
int gui_getch(void);
int gui_peekch(void);
int gui_prompt(const char *p, char *buf, int siz);

void gui_status( const char *, ...);
void gui_statusl(const char *, va_list);
void gui_status_addl(const char *s, va_list l);
void gui_status_add(const char *s, ...);

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

void gui_drawbuffer(buffer_t *b);

int  gui_scroll(enum scroll);
void gui_inc(int n);
void gui_dec(int n);
void gui_move(int y, int x);
void gui_move_motion(struct motion *);

void gui_clip(void);
void gui_draw(void);
void gui_redraw(void);

#define CTRL_AND(c)  ((c) & 037)
#define SCROLL_OFF 3

#endif
