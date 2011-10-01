#include <ncurses.h>

#include "syntax.h"

static int quoting = 0;

void gui_syntax(char c, int on)
{
#define off !on
	switch(c){
		case '"':
		case '\'':
			if(!quoting){
				if(on){
					attron(COLOR_PAIR(1 + COLOR_RED));
					quoting = 2;
				}
			}else if(off && --quoting == 0){
				attroff(COLOR_PAIR(1 + COLOR_RED));
				quoting = 0;
			}
			break;

		case '#':
			if(on)
				attron(COLOR_PAIR(1 + COLOR_YELLOW));
			break;
	}
}

void gui_syntax_reset(void)
{
	quoting = 0;
}
