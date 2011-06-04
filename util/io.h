#ifndef IO_H
#define IO_H

struct list *fgetlines(FILE *f, int *haseol);
void chomp_line(void);
void *readfile(const char *filename, int ro);

#endif
