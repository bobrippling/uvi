#include <stdio.h>
#include <stdlib.h>

#include "util/alloc.h"
#include "range.h"
#include "util/list.h"
#include "yank.h"

static struct yank yanks[1 + YANK_CHAR_LAST - YANK_CHAR_FIRST]; /* named regs + default */

static int yank_index(char reg)
{
	if(yank_char_valid(reg))
		return reg - YANK_CHAR_FIRST;
	return -1;
}

struct yank *yank_get(char reg)
{
	int i = yank_index(reg);
	if(i == -1)
		return &yanks[0]; /* eh.. */
	return &yanks[i];
}

static void yank_set(char reg, void *d, int is_l)
{
	int i = yank_index(reg);

	if(i == -1)
		return;

	if(yanks[i].v){
		if(yanks[i].is_list)
			list_free((struct list *)yanks[i].v, free);
		else
			free(yanks[i].v);
	}

	yanks[i].v = d;
	yanks[i].is_list = is_l;
}

void yank_set_str(char reg, char *s)
{
	yank_set(reg, s, 0);
}

void yank_set_list(char reg, struct list *l)
{
	yank_set(reg, l, 1);
}

int yank_char_valid(int reg)
{
	return YANK_CHAR_FIRST <= reg && reg <= YANK_CHAR_LAST;
}
