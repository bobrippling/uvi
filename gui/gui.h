#ifndef GUI_H
#define GUI_H

int  gui_init();
void gui_term();

void gui_addch(int c);
void gui_mvaddch(int y, int x, int c);

int gui_getstr(char *s, int size);
int gui_getch();

void gui_status( const char *, ...);
void gui_statusl(const char *, va_list);

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
void gui_move(struct motion *);

void gui_refresh(void);

#define CTRL_AND(c)  ((c) & 037)

#endif
