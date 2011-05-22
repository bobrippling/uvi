#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"
#include "global.h"

#define LEN(x) (sizeof(x) / sizeof(x[0]))

static struct
{
	const char *nam;
	int isbool;
	int *val;
} vars[] = {
	[VARS_READONLY]   = { "ro",       1, NULL },
	[VARS_MODIFIED]   = { "modified", 1, NULL },
	[VARS_EOL]        = { "eol",      1, NULL },
	[VARS_TABSTOP]    = { "ts",       0, &global_settings.tabstop },
	[VARS_SHOWTABS]   = { "st",       1, &global_settings.showtabs },
	[VARS_AUTOINDENT] = { "ai",       1, &global_settings.autoindent },
};

int vars_set(enum vartype t, buffer_t *b, int v)
{
	int *p = vars_get(t, b);

	*p = v;
	if(t == VARS_EOL)
		buffer_modified(b) = 1;

	return 1;
}

int vars_isbool(enum vartype t)
{
	return vars[t].isbool;
}

int vars_isbuffervar(enum vartype t)
{
	return !vars[t].val;
}

int *vars_get(enum vartype t, buffer_t *b)
{
	int *p = vars[t].val;
	if(p)
		return p;

	switch(t){
		case VARS_READONLY:
			return &buffer_readonly(b);
		case VARS_MODIFIED:
			return &buffer_modified(b);
		case VARS_EOL:
			return &buffer_eol(b);

		default:
			break;
	}
	return NULL;
}

int *vars_settingget(enum vartype t)
{
	return vars[t].val;
}

const char *vars_tostring(enum vartype t)
{
	return vars[t].nam;
}

enum vartype vars_gettype(const char *s)
{
	int i;
	for(i = 0; i < LEN(vars); i++)
		if(vars[i].nam && !strcmp(vars[i].nam, s))
			return i;
	return VARS_UNKNOWN;
}
