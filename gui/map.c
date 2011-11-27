#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../range.h"
#include "../buffer.h"
#include "gui.h"
#include "map.h"
#include "../range.h"
#include "../util/list.h"
#include "../util/alloc.h"
#include "../str/str.h"


static struct list *maps = NULL;

void map_add(char c, const char *cmd)
{
	struct map *m = umalloc(sizeof *m);

	m->c = c;
	m->cmd = ustrdup(cmd);
	str_escape(m->cmd);

	list_append(maps, m);
}

void map()
{
	struct list *l;
	int found = 0;
	int c = gui_getch(GETCH_COOKED);

	for(l = maps; l && l->data; l = l->next){
		struct map *m = l->data;

		if(m->c == c){
			gui_queue(m->cmd);
			found = 1;
			/* keep looking for more */
		}
	}

	if(!found && c != CTRL_AND('['))
		gui_status(GUI_ERR, "map %c not defined", c);
}

void map_init()
{
	maps = list_new(NULL);
}

void map_free(void *p)
{
	free(((struct map *)p)->cmd);
	free(p);
}

void map_term()
{
	list_free(maps, map_free);
}

struct list *map_getall()
{
	return maps;
}
