#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gui.h"
#include "macro.h"

static char *macros[26];

void macro_set(char c, char *s)
{
	int i = c - 'a';

	if(macros[i])
		free(macros[i]);

	macros[i] = s;
}

void macro_play(char c)
{
	const char *iter = macros[c - 'a'];
	if(iter){
		int last = strlen(iter) - 1;
		if(iter[last] != '\x1b')
			gui_ungetch('\x1b'); /* return to normal */
		gui_queue(iter);
	}
}

int macro_char_valid(int c)
{
	return 'a' <= c && c <= 'z';
}
