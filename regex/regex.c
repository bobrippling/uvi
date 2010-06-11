#include <errno.h>

#include "regex.h"

/*
 * char *str;
 * enum { NONE, STAR, PLUS } noderepeat;
 * struct _regex *next;
 */

regex_t *regex_create(const char *s)
{
	regex_t *r = malloc(sizeof(*r));
	char *start = s;
	int len;

	if(!r)
		return NULL;

	errno = 0;

	while(*s && *s != '*' && *s != '+')
		s++;

	len = s - start;

	if(len){
		r->str = malloc(len + 1);
		if(!r->str)
			goto membail;

		strncpy(r->str, start, len);

		if(*s == '\0'){
			r->next = malloc(sizeof(*(cur->next)));
			if(!r->next)
				goto membail;
		}
	}else if(*s == '\0'){
		free(r);
		return NULL;
	}else
		r->str = NULL;

	switch(*s){
		case '*':
		case '+':
			if(!len){
				errno = EINVAL;
				return NULL;
			}
			r->noderepeat = *s == '*' ? STAR : PLUS;
			break;

		case '\0':
			r->noderepeat = NONE;
			break;

		default:
			fprintf("MAJOR REGEX ERROR: '%c'\n", *s);
			return NULL;
	}

	r->next = regex_create(s);

	return r;
membail:
	errno = ENOMEM;
	regex_free(r);
}

int regex_matches(regex_t *r, const char *s, int *start, int *end)
{
	return 0;
}

/* 1 matches on 2 starting at 3, ending at 4 */
void regex_free(regex_t *r)
{
	while(r){
		regex_t *tmp = r;
		r = r->next;

		free(tmp->str);
		free(tmp);
	}
}
