#ifndef IO_H
#define IO_H

void chomp_line(void);
void *readfile(const char *filename);
void input_reopen(void);
#ifdef BUFFER_H
void dumpbuffer(buffer_t *b);
#endif

#endif
