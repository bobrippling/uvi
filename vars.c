#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"
#include "global.h"

#define S_READONLY     "ro"
#define S_MODIFIED     "modified"
#define S_EOL          "eol"
#define S_TABSTOP      "ts"
#define S_SHOWTABS     "st"
#define S_AUTOINDENT   "ai"
#if VIEW_COLOUR
#define S_COLOUR       "colour"
#endif

int vars_set(enum vartype t, buffer_t *b, int v)
{
	int *p = vars_bufferget(t, b);
	if(!p){
		p = vars_settingget(t);
		if(!p)
			return 0;
	}

	*p = v;

	if(t == VARS_EOL)
		buffer_modified(b) = 1;

	return 1;
}

int vars_isbool(enum vartype v)
{
	switch(v){
		case VARS_READONLY:
		case VARS_MODIFIED:
		case VARS_EOL:
		case VARS_SHOWTABS:
		case VARS_AUTOINDENT:
#if VIEW_COLOUR
		case VARS_COLOUR:
			return 1;
#endif
		case VARS_TABSTOP:
			return 0;
		case VARS_UNKNOWN:
			break;
	}
	return -1;
}

int vars_isbuffervar(enum vartype t)
{
	switch(t){
	 case VARS_READONLY:
		case VARS_MODIFIED:
		case VARS_EOL:
			return 1;
		case VARS_UNKNOWN:
		case VARS_TABSTOP:
		case VARS_SHOWTABS:
		case VARS_AUTOINDENT:
#if VIEW_COLOUR
		case VARS_COLOUR:
#endif
			break;
	}
	return 0;
}

int *vars_bufferget(enum vartype t, buffer_t *b)
{
	switch(t){
		case VARS_READONLY:
			return &buffer_readonly(b);
		case VARS_MODIFIED:
			return &buffer_modified(b);
		case VARS_EOL:
			return &buffer_eol(b);
		case VARS_SHOWTABS:
		case VARS_TABSTOP:
		case VARS_AUTOINDENT:
#if VIEW_COLOUR
		case VARS_COLOUR:
#endif
		case VARS_UNKNOWN:
			break;
	}
	return NULL;
}

int *vars_settingget(enum vartype t)
{
	extern struct settings global_settings;

	switch(t){
		case VARS_TABSTOP:
			return &global_settings.tabstop;

		case VARS_SHOWTABS:
			return &global_settings.showtabs;

		case VARS_AUTOINDENT:
			return &global_settings.autoindent;

#if VIEW_COLOUR
		case VARS_COLOUR:
			return &global_settings.colour;
#endif

		case VARS_READONLY:
		case VARS_MODIFIED:
		case VARS_EOL:
		case VARS_UNKNOWN:
			break;
	}
	return NULL;
}

int *vars_get(enum vartype t, buffer_t *b)
{
	int *p = vars_bufferget(t, b);
	if(!p)
		return vars_settingget(t);
	return p;
}

const char *vars_tostring(enum vartype v)
{
	switch(v){
		case VARS_SHOWTABS:
			return S_SHOWTABS;
		case VARS_READONLY:
			return S_READONLY;
		case VARS_MODIFIED:
			return S_MODIFIED;
		case VARS_EOL:
			return S_EOL;
		case VARS_TABSTOP:
			return S_TABSTOP;
		case VARS_AUTOINDENT:
			return S_AUTOINDENT;
#if VIEW_COLOUR
		case VARS_COLOUR:
			return S_COLOUR;
#endif
		case VARS_UNKNOWN:
			break;
	}
	return NULL;
}

enum vartype vars_gettype(const char *s)
{
	if(!strcmp(S_READONLY, s))
		return VARS_READONLY;
	else if(!strcmp(S_SHOWTABS, s))
		return VARS_SHOWTABS;
	else if(!strcmp(S_MODIFIED, s))
		return VARS_MODIFIED;
	else if(!strcmp(S_EOL, s))
		return VARS_EOL;
	else if(!strcmp(S_TABSTOP, s))
		return VARS_TABSTOP;
#if VIEW_COLOUR
	else if(!strcmp(S_COLOUR, s))
		return VARS_COLOUR;
#endif

	return VARS_UNKNOWN;
}
