#include <ncurses.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>

#include "buffer.h"
#include "range.h"
#include "command.h"
#include "list.h"
#include "main.h"

#include "ncurses.h"
#include "config.h"

#define CTRL_AND(c) ((c) & 037)
#define C_ESC       27
#define C_EOF       4
#define C_DEL       127
#define C_BACKSPACE 263
#define C_NEWLINE   '\r'
#define C_CTRL_C    3

static void nc_up(void);
static void nc_down(void);
static void sigh(int);

static void pfunc(const char *, ...);
static void wrongfunc(void);
static int  qfunc(const char *);
static char *gfunc(char *, int);

static int colon(void);
static void status(const char *);

static buffer_t *buffer;
static int curline = 1, maxy, maxx, saved = 1;

static void nc_down()
{
  endwin();
}

static void nc_up()
{
  static int init = 0;

  if(!init){
    initscr();
    /*cbreak(); * use raw() to intercept ^C, ^Z */
    raw();
    noecho();

    /* halfdelay() for main-loop style timeout */

    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);

    getmaxyx(stdscr, maxy, maxx);
    --maxy;
    --maxx;

    init = 1;
  }
  refresh();
}

static void status(const char *s)
{
  mvaddstr(maxy, 0, s);
}

static void pfunc(const char *s, ...)
{
  va_list l;
  va_start(l, s);
  move(maxy, 0);
  vwprintw(stdscr, s, l);
  va_end(l);
  addch('\n');
}

static int qfunc(const char *s)
{
  int c;
  status(s);
  c = getch();
  return c == '\n' || tolower(c) == 'y';
}

static char *gfunc(char *s, int size)
{
  /*return getnstr(s, len) == OK ? s : NULL;*/
	int x, y, c, count = 0;
	enum { CANCEL, SUCCESS, FAILURE } ret;
	getyx(stdscr, y, x);
  clrtoeol();

	do
		switch((c = getch())){
			case C_EOF:
				if(count){
			case C_NEWLINE:
			case '\n':
					s[count] = '\0';
					ret = SUCCESS;
					goto exit;
				}else{
					ret = CANCEL;
					goto exit;
				}

			case C_DEL:
			case C_BACKSPACE:
				if(!count){
					ret = CANCEL;
					goto exit;
				}
				count--;
				move(y, --x);
				break;

			case C_ESC:
				ret = CANCEL;
				goto exit;

			default:
				if(isprint(c)){
					s[count++] = c;
					addch(c);
					x++;
					if(count >= size-1){
						s[count] = '\0';
						ret = SUCCESS;
						goto exit;
					}
				}else{
					ret = CANCEL;
					goto exit;
				}
		}
	while(1);

exit:
	move(0, 0);

	switch(ret){
		case CANCEL:
			*s = '\0';
		case SUCCESS:
			return s;
		case FAILURE:
			break;
	}
	return NULL;
}

static void wrongfunc(void)
{
	move(maxy, 0);
	clrtoeol();
	addch('?');
}

static int colon()
{
#define BUF_SIZE 128
  char in[128];
  mvaddch(maxy, 0, ':');
  if(gfunc(in, BUF_SIZE))
		return runcommand(in, buffer,
          &curline, &saved,
          &wrongfunc, &pfunc,
          &gfunc, &qfunc);

  return 1;
#undef BUF_SIZE
}

static void sigh(int sig)
{
	nc_down();
	bail(sig);
}

int ncurses_main(const char *filename)
{
  int c;

	signal(SIGINT, &sigh);

  nc_up();

	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			if(errno == ENOENT)
				goto new_file;

      nc_down();
			fprintf(stderr, PROG_NAME": %s: ", filename);
      perror(NULL);
			return 1;
		}else if(nread == 0)
			status("(empty file)\n");
		else
			pfunc("%s: %dC, %dL%s\n", filename,
          buffer_nchars(buffer), buffer_nlines(buffer),
          buffer->haseol ? "" : " (noeol)");
	}else{
new_file:
		buffer = buffer_new();
		status("(new file)");
	}


  do
    switch((c = getch())){
      case 'Z':
        if(getch() == 'Z')
          goto exit_while;
        break;

      case ':':
        if(!colon())
          goto exit_while;
        break;

			case C_CTRL_C:
				sigh(SIGINT);
				break;

      default:
        mvprintw(1, 2, "unknown: %c (%d)", c, c);
    }
  while(1);
exit_while:


  nc_down();
  return 0;
}
