#ifndef VARS_H
#define VARS_H

char	vars_set(buffer_t *, const char *, const char);
char *vars_get(buffer_t *, const char *);

enum varlist
{
	VARS_READONLY, VARS_MODIFIED, VARS_EOL, VARS_SENTINEL
};

const char *vars_tostring(enum varlist);

#endif
