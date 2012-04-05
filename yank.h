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

#define YANK_CHAR_FIRST ('a' - 1)
#define YANK_CHAR_LAST   'z'
#define YANK_CHAR_ANON YANK_CHAR_FIRST

#define ITER_YANKS(i) for(i = YANK_CHAR_FIRST; i <= YANK_CHAR_LAST; i++)

#endif
