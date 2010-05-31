#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"

#define S_READONLY "ro"
#define S_MODIFIED "modified"
#define S_EOL      "eol"

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
	if(!strcmp(S_READONLY, s))
		return &buffer_readonly(b);
	else if(!strcmp(S_MODIFIED, s))
		return &buffer_modified(b);
	else if(!strcmp(S_EOL, s))
		return &buffer_eol(b);

	return NULL;
}

const char *vars_tostring(enum varlist v)
{
	switch(v){
		case VARS_READONLY:
			return S_READONLY;
		case VARS_MODIFIED:
			return S_MODIFIED;
		case VARS_EOL:
			return S_EOL;
		case VARS_SENTINEL:
			break;
	}
	return NULL;
}
