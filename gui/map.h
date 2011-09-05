#ifndef MAP_H
#define MAP_H

struct map
{
	char c;
	char *cmd;
};


void map(void);
void map_add(char c, const char *cmd);
void map_init(void);
void map_term(void);

struct list *map_getall();

#endif
