#ifndef VARS_H
#define VARS_H

enum vartype
{
	VARS_READONLY,
	VARS_MODIFIED,
	VARS_EOL,
	VARS_TABSTOP,
	VARS_UNKNOWN
} vars_gettype(const char *);

char vars_set(enum vartype, buffer_t *, const char);

char vars_isbuffervar(enum vartype);
char vars_isbool(enum vartype);

char *vars_bufferget(enum vartype, buffer_t *);
char *vars_settingget(enum vartype);
char *vars_get(enum vartype, buffer_t *);


const char *vars_tostring(enum vartype);

#endif
