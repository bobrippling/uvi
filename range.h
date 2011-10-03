#ifndef RANGE_H
#define RANGE_H

struct range
{
	int start, end;
};

char *parserange(char *, struct range *, struct range *);

#endif
