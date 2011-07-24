#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <time.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"
#include "global.h"
#include "gui/motion.h"
#include "gui/intellisense.h"
#include "gui/gui.h"
#include "buffers.h"

#define LEN(x) ((sizeof(x) / sizeof(x[0])))

static struct
{
	const char *nam;
	int isbool;
	int *val;
} vars[] = {
	[VARS_READONLY]   = { "ro",       1, NULL },
	[VARS_MODIFIED]   = { "modified", 1, NULL },
	[VARS_EOL]        = { "eol",      1, NULL },
	[VARS_CRLF]       = { "crlf",     1, NULL },
	[VARS_TABSTOP]    = { "ts",       0, &global_settings.tabstop },
	[VARS_SHOWTABS]   = { "st",       1, &global_settings.showtabs },
	[VARS_LIST]       = { "list",     1, &global_settings.list },
	[VARS_AUTOINDENT] = { "ai",       1, &global_settings.autoindent },
	[VARS_TEXTWIDTH]  = { "tw",       0, &global_settings.textwidth },
	[VARS_FSYNC]      = { "fsync",    1, &global_settings.fsync },
};

void vars_set(enum vartype t, buffer_t *b, int v)
{
	int *p = vars_addr(t, b);

	*p = v;
	if(t == VARS_EOL || t == VARS_CRLF)
		buffer_modified(b) = 1;
}

int vars_isbool(enum vartype t)
{
	return vars[t].isbool;
}

int vars_isbuffervar(enum vartype t)
{
	return !vars[t].val;
}

int vars_get(enum vartype t, buffer_t *b)
{
	return *vars_addr(t, b);
}

int *vars_addr(enum vartype t, buffer_t *b)
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
		case VARS_CRLF:
			return &buffer_crlf(b);

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
	unsigned int i;
	for(i = 0; i < LEN(vars); i++)
		if(vars[i].nam && !strcmp(vars[i].nam, s))
			return i;
	return VARS_UNKNOWN;
}

void vars_show(enum vartype t)
{
	if(vars_isbool(t)){
		gui_status_add_col(vars_get(t, buffers_current()) ? "" : "no", GUI_COL_RED, vars_tostring(t), GUI_NONE, NULL);
	}else{
		char buf[8];
		snprintf(buf, sizeof buf, "%d", vars_get(t, buffers_current()));
		gui_status_add_col(vars_tostring(t), GUI_COL_BLUE, "=", GUI_NONE, buf, GUI_COL_GREEN, NULL);
	}
}
