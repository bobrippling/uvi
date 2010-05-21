#ifndef PARSE_H
#define PARSE_H

#define RANGE_FIRST -1
/* need limits.h for this */
#define RANGE_LAST  INT_MAX

struct range
{
	int start, end;
};

char *parserange(char *, struct range *);


#endif
