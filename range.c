#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include "parse.h"

static int curv;

static int number(char **);

static int number(char **sp)
{
	char *s = *sp;

	if(isdigit(*s)){
		curv = *s - '0';
		s++;
		++*sp;
		while(isdigit(*s)){
			curv = curv * 10 + *s - '0';
			s++;
			++*sp;
		}
		return 1;
	}else if(*s == '^'){
		++*sp;
		curv = RANGE_FIRST;
		return 1;
	}else if(*s == '$'){
		++*sp;
		curv = RANGE_LAST;
		return 1;
	}

	return 0;
}


char *parserange(char *s, struct range *r)
{
	if(number(&s)){
		int start = curv;

		if(*s == ','){
			++s;
			if(number(&s)){
				r->start = start;
				r->end   = curv;
				return s;
			}else
				return NULL;
		}else{
			r->start = start;
			r->end   = start;
			return s;
		}
	}else if(*s == '%'){
		r->start = RANGE_FIRST;
		r->end   = RANGE_LAST;
		return s + 1;
	}else
		return s;
}
