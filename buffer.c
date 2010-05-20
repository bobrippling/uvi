#include <stdio.h>
#include "buffer.h"

buffer_t *buffer_read(const char *);
int			 buffer_write(buffer_t *);
char		**buffer_getname(buffer_t *);

struct list *buffer_getlines(buffer_t *);
