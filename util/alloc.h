#ifndef ALLOC_H
#define ALLOC_H

void *umalloc(size_t);
void *urealloc(void *, size_t);
char *ustrdup(const char *);
void ustrcat(char **p, int *siz, ...);
char *ustrprintf(const char *fmt, ...);

#ifdef UVI_ALLOCA
void *uvi_alloca(size_t);
# define ALLOCA uvi_alloca
#else
# define ALLOCA alloca
#endif

#endif
