#include <ncurses.h>
#include <errno.h>

#include "buffer.h"

#include "ncurses.h"
#include "config.h"

static void nc_up(void);
static void nc_down(void);

static buffer_t *buffer;

static void nc_down()
{
  endwin();
}

static void nc_up()
{
  static int init = 0;

  if(!init){
    initscr();
    cbreak(); /* use raw() to intercept ^C, ^Z */
    noecho();

    /* halfdelay() for main-loop style timeout */

    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);

    init = 1;
  }else
    refresh();
}


int ncurses_main(const char *filename)
{
	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			if(errno == ENOENT)
				goto new_file;

			printw(PROG_NAME": %s: ", filename);
			perror(NULL);
			return 1;
		}else if(nread == 0)
			fputs("(empty file)\n", stderr);
		else
			printf("%s: %dC, %dL%s\n", filename, buffer_nchars(buffer),
					buffer_nlines(buffer), buffer->haseol ? "" : " (noeol)");
	}else{
new_file:
		buffer = buffer_new();
		puts("(new file)");
	}
  nc_up();

  nc_down();
  return 0;
}
