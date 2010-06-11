#ifndef REGEX_H
#define REGEX_H

typedef struct
	{
		char *str;
		enum { NONE, STAR, PLUS } noderepeat;
		struct _regex *next;
	} regex_t;


regex_t *regex_create(const char *);
int      regex_matches(regex_t *, const char *, int *, int *);
/* 1 matches on 2 starting at 3, ending at 4 */
void     regex_free(regex_t *);

#endif
