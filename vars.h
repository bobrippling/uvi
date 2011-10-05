#ifndef VARS_H
#define VARS_H

enum vartype
{
	VARS_READONLY,
	VARS_MODIFIED,

	VARS_EOL,
	VARS_CRLF,

	VARS_TABSTOP,
	VARS_SHOWTABS,
	VARS_LIST,
	VARS_SCROLLOFF,
	VARS_TEXTWIDTH,
	VARS_TAB_CONTEXT,

	VARS_ICASE,

	VARS_HIGHLIGHT,
	VARS_SYNTAX,

	VARS_AUTOINDENT,
	VARS_CINDENT,
	VARS_FUNC_MOTION_VI,
	VARS_EXPANDTAB,

	VARS_FSYNC,
	VARS_WTRIM,

	VARS_UNKNOWN
} vars_gettype(const char *);

void vars_set(enum vartype, buffer_t *, int);

int vars_isbuffervar(enum vartype);
int vars_isbool(enum vartype);

int *vars_bufferget(enum vartype, buffer_t *);
int *vars_settingget(enum vartype);
int *vars_addr(enum vartype, buffer_t *);
int  vars_get( enum vartype, buffer_t *);


const char *vars_tostring(enum vartype);
void vars_show(enum vartype t);
void vars_help(enum vartype t);

void vars_default(void);

#endif
