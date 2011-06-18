#ifndef PIPE_H
#define PIPE_H

struct list *pipe_read(const char *);
int          pipe_write(const char *, struct list *, int close_out);
struct list *pipe_readwrite(const char *, struct list *);

#endif
