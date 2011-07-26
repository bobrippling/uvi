#ifndef BUFFERS_H
#define BUFFERS_H

void buffers_init(int, const char **, int ro);
void buffers_term();

buffer_t    *buffers_current();
const char **buffers_array();

int          buffers_idx();
int          buffers_count();

int          buffers_next(int n);
int          buffers_goto(int n);

/*
 * load the filename (or return error)
 * adding to the buffer list, unless already present
 */
void         buffers_load(const char *);

#endif
