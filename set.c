#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "map.h"
#include "alloc.h"
#include "set.h"

static map *variables = NULL;

void set_set(const char *s, const char v)
{
	char *dup = umalloc(strlen(s) + 1), *vdup = umalloc(sizeof(*vdup));

	strcpy(dup, s);
	*vdup = v;

	map_add(variables, dup, vdup);
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
}

void set_term()
{
	map_free(variables);
}
