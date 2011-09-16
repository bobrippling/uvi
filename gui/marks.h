#ifndef MARKS_H
#define MARKS_H

void mark_set(int, int, int);
int  mark_get(int, int *, int *);
int  mark_is_set(int);
int  mark_count(void);
int  mark_find(int y, int x);

void mark_jump(void);
void mark_edit(void);

#define mark_valid(c) (('a' <= (c) && (c) <= 'z') || ('0' <= (c) && (c) <= '9') || (c) == '\'' || (c) == '.')

#endif
