#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "alloc.h"
#include "../main.h"

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

void *urealloc(void *p, size_t s)
{
	void *n = realloc(p, s);
	if(!n)
		die("realloc()");
	return n;
}

char *ustrcat(const char *first, ...)
{
	int len = strlen(first);
	va_list l;
	const char *s;
	char *ret;

	va_start(l, first);
	while((s = va_arg(l, const char *)))
		len += strlen(s);
	va_end(l);

	ret = umalloc(len + 1);

	strcpy(ret, first);
	va_start(l, first);
	while((s = va_arg(l, const char *)))
		strcat(ret, s);
	va_end(l);

	return ret;
}
