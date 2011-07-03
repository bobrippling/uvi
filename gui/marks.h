#ifndef MARKS_H
#define MARKS_H

void mark_set(int, int, int);
int  mark_get(int, int *, int *);
int  mark_isset(int);
void mark_set_last(int, int);

#define mark_valid(c) (('a' <= (c) && (c) <= 'z') || (c) == '\'')

#endif
