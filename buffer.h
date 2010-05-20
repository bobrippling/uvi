#ifndef FILE_H
#define FILE_H

typedef struct
  {
    struct list *lines;
    /* jump list */
    char *fname;
  } buffer_t;

buffer_t *buffer_read(const char *);
int       buffer_write(buffer_t *);
char    **buffer_getname(buffer_t *);

struct list *buffer_getlines(buffer_t *);

#endif
