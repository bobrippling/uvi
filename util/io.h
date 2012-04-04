#ifndef IO_H
#define IO_H

void input_reopen(void);
void chomp_line(void);
#ifdef BUFFER_H
void dumpbuffer(buffer_t *b);
#endif
char *fline(FILE *f, int *eol);

#endif
