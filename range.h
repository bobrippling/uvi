#ifndef PARSE_H
#define PARSE_H

struct range
{
	int start, end;
};

char *parserange(char *, struct range *, struct range *);

#endif
