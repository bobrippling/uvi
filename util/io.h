#ifndef IO_H
#define IO_H

struct list *fgetlines(FILE *f, int *haseol);
struct list *fnamegetlines(const char *s, int *haseol);
void chomp_line(void);
void *readfile(const char *filename);

#endif
