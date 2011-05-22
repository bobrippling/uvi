#ifndef MARKS_H
#define MARKS_H

void mark_set(int, int, int);
int  mark_get(int, int *, int *);
int  mark_isset(int);

#define mark_valid(c) ('a' <= (c) && (c) <= 'z')

#endif
