#ifndef YANK_H
#define YANK_H

struct yank
{
	void *v;
	int is_list;
};

struct yank *yank_get(     char reg);
void         yank_set_str( char reg, char *);
void         yank_set_list(char reg, struct list *);

int yank_char_valid(int c);

#endif
