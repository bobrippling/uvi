#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "range.h"

extern void *stderr;

static int	 number(char **, int, int);
static char *getrange(char *, struct range *, int, int);

static int curv;

static int number(char **sp, int cur, int lim)
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
	}else
		switch(*s){
			case '^':
				++*sp;
				curv = 1;
				return 1;

			case '$':
				++*sp;
				curv = lim;
				return 1;

			case '.':
				++*sp;
				curv = cur+1;
				return 1;

			case '+':
			case '-':
			{
				int chr = *s;
				int i;

				++*sp;

				if(isdigit(**sp)){
					number(sp, cur, lim);
					i = curv;
				}else{
					i = cur;
				}

				if(chr == '+')
					i += 2;
				else
					i -= 1;
				curv = i;

fprintf(stderr, "parsed, curv=%d (i=%d, chr=%c), *sp=\"%s\"\n",curv,i,chr,*sp);

				return 1;
			}
		}

	return 0;
}

static char *getrange(char *s, struct range *r, int cur, int lim)
{
	if(number(&s, cur, lim)){
		int start = curv;

		if(*s == ','){
			++s;
			if(number(&s, cur, lim)){
				r->start = start;
				r->end	 = curv;
				return s;
			}else
				return NULL;
		}else{
			r->start = start;
			r->end	 = start;
			return s;
		}
	}else if(*s == '%'){
		r->start = 1;
		r->end	 = lim;
		return s + 1;
	}else
		return s;
}

char *parserange(char *in, struct range *rng, struct range *lim)
{
	char *s = getrange(in, rng, lim->start, lim->end);

	if(s > in){
		/* validate range */
fprintf(stderr, "rng = { %d, %d }, cur=%d, max=%d\n",
	rng->start, rng->end,
	lim->start, lim->end);

		if(rng->start < 1 || rng->start > lim->end ||
			 rng->end < 1 || rng->end > lim->end){
			errno = EINVAL;
			return NULL;
		}else if(rng->start > rng->end){
			errno = ERANGE;
			return NULL;
		}
	}
	return s;
}
