#include <stdlib.h>
#include <setjmp.h>

#include "alloc.h"

void *umalloc(size_t s)
{
	void *p = malloc(s);
	if(!p)
		longjmp(allocerr, ALLOC_UMALLOC);
	return p;
}
