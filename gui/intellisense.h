#ifndef INTELLISENSE_H
#define INTELLISENSE_H

typedef int (*intellisensef)(char **, int *, int *, char);

int intellisense_insert(char **pstr, int *psize, int *pos, char ch);
int intellisense_file(  char **pstr, int *psize, int *pos, char ch);

enum intellisense_opt
{
	INTELLI_NONE, INTELLI_INS, INTELLI_CMD
};

void intellisense_init_opt(void *opts, enum intellisense_opt);

#endif
