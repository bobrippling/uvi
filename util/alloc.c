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

void ustrcat(char **p, int *siz_arg, ...)
{
	int len;
	va_list l;
	const char *s;
	int *siz = siz_arg ? siz_arg : &len;

	len = *p ? strlen(*p) + 1 : 0;

	va_start(l, siz_arg);
	while((s = va_arg(l, const char *)))
		len += strlen(s);
	va_end(l);

	if(*siz <= len){
		/* not enough room */
		*p = realloc(*p, *siz = len + 1);
		if(!*p)
			die("ustrcat()");
	}

	va_start(l, siz_arg);
	while((s = va_arg(l, const char *)))
		strcat(*p, s);
	va_end(l);
}
