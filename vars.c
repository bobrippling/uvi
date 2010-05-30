#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"

#define VARS_READONLY "ro"
#define VARS_MODIFIED "modified"
#define VARS_EOL      "eol"


char vars_set(buffer_t *b, const char *s, const char v)
{
	char *p = vars_get(b, s);
	if(!p)
		return 0;
	*p = v;
	return 1;
}

char *vars_get(buffer_t *b, const char *s)
{
	if(!strcmp(VARS_READONLY, s))
		return &buffer_readonly(b);
	else if(!strcmp(VARS_MODIFIED, s))
		return &buffer_modified(b);
	else if(!strcmp(VARS_EOL, s))
		return &buffer_eol(b);

	return NULL;
}
