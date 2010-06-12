#ifndef PARSE_H
#define PARSE_H

struct range
{
	int start, end;
};

/*								 range, passed back range, limits, question func */
char *parserange(char *, struct range *, struct range *,
		int	(*)(const char *, ...), /* qfunc, pfunc */
		void (*)(const char *, ...));

#endif
