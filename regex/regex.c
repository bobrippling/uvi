#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "regex.h"

/*
 * char *str;
 * enum { NONE, STAR, PLUS } noderepeat;
 * struct _regex *next;
 */

regex_t *regex_create(const char *s)
{
	regex_t *r = malloc(sizeof(*r));
	const char *start = s;
	int len;

	printf("regex_create(\"%s\")\n", s);
	fflush(stdout);


	if(!r)
		return NULL;

	errno = 0;

	while(*s && *s != '*' && *s != '+' && *s != '?')
		s++;

	len = s - start;

	if(len){
		r->str = malloc(len + 1);
		if(!r->str)
			goto membail;

		strncpy(r->str, start, len);

		if(*s == '\0'){
			r->next = malloc(sizeof(*r));
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
			r->noderepeat = STAR;
			break;

		case '+':
			r->noderepeat = PLUS;
			break;

		case '?':
			r->noderepeat = QMARK;
			break;

		case '\0':
			r->noderepeat = NONE;
			break;

		default:
			fprintf(stderr, "MAJOR REGEX ERROR: '%c'\n", *s);
			return NULL;
	}

	if(r->noderepeat != NONE && !len){
		errno = EINVAL;
		return NULL;
	}

	if(*s == '\0')
		r->next = NULL;
	else
		r->next = regex_create(s + 1);

	return r;
membail:
	errno = ENOMEM;
	regex_free(r);
	return NULL;
}

/* 1 matches on 2 starting at 3, ending at 4 */
int regex_matches(regex_t *r, const char *s, int *start, int *end)
{
	char *nodepos;
	int i = strlen(nodepos) - 1;

try:
	nodepos = r->str;

	do{
		if(*s == *nodepos){
			nodepos++;
			s++;
		}else
			return 0;
		i--;
	}

	switch(r->noderepeat){
		case NONE:
			return *s == *nodepos;

		case STAR:
			while(*s == *nodepos){
				s++;
				nodepos++;
			}
			if(*nodepos == '\0')
			break;


	return 1;
}

void regex_free(regex_t *r)
{
	while(r){
		regex_t *tmp = r;
		r = r->next;

		free(tmp->str);
		free(tmp);
	}
}
