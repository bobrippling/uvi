#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "alloc.h"

void *umalloc(size_t s)
{
	void *p = malloc(s);
	if(!p)
		longjmp(allocerr, ALLOC_UMALLOC);
	return p;
}

char *ustrdup(const char *s)
{
	char *d = umalloc(strlen(s) + 1);
	strcpy(d, s);
	return d;
}
