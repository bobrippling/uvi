#include <stdlib.h>
#include <string.h>

#include "alloc.h"

void *umalloc(size_t s)
{
	void *p = malloc(s);
	if(!p)
		die("umalloc()");
	return p;
}

char *ustrdup(const char *s)
{
	char *d = umalloc(strlen(s) + 1);
	strcpy(d, s);
	return d;
}
