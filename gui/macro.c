#include <stdlib.h>
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
		extern void *stderr;
		fprintf(stderr, "macro_play: gui_queue(\"%s\")\n", iter);
		gui_queue(iter);
	}
}

int macro_char_valid(int c)
{
	return 'a' <= c && c <= 'z';
}
