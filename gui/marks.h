#ifndef MARKS_H
#define MARKS_H

void mark_set(int, int, int);
int  mark_get(int, int *, int *);
int  mark_is_set(int);

void mark_jump(void);
void mark_edit(void);

#define mark_valid(c) (('a' <= (c) && (c) <= 'z') || (c) == '\'' || (c) == '.')

#endif
