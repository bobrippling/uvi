#include <stdio.h>
#include <stdlib.h>

#include "util/alloc.h"
#include "range.h"
#include "util/list.h"
#include "yank.h"

static struct yank yanks[27]; /* named regs + default */

static int yank_index(char reg)
{
	if('a' <= reg && reg <= 'z')
		return reg - 'a';
	return 26;
}

struct yank *yank_get(char reg)
{
	return &yanks[yank_index(reg)];
}

static void yank_set(char reg, void *d, int is_l)
{
	int i = yank_index(reg);

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
	return !reg || ('a' <= reg && reg <= 'z');
}
