#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "util/map.h"
#include "util/alloc.h"
#include "set.h"

static map *variables = NULL;

char set_set(const char *s, const char v)
{
	if(!strcmp(s, SET_READONLY) || !strcmp(s, SET_MODIFIED) ||
			!strcmp(s, SET_EOF)){
		char *dup = umalloc(strlen(s) + 1), *vdup = umalloc(sizeof(*vdup));

		strcpy(dup, s);
		*vdup = v;

		map_add(variables, dup, vdup);
		return 1;
	}
	return 0;
}

char *set_get(char *s)
{
	return map_get(variables, s);
}

void set_init()
{
	variables = map_new();
	set_set(SET_READONLY, 0);
	set_set(SET_MODIFIED, 0);
	set_set(SET_EOF,			0);
}

void set_term()
{
	map_free(variables);
}
