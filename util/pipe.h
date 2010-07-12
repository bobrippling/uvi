#ifndef PIPE_H
#define PIPE_H

struct list *pipe_read(const char *);
int pipe_write(const char *, struct list *);

#endif
