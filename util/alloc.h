#ifndef ALLOC_H
#define ALLOC_H

void *umalloc(size_t);
void *urealloc(void *, size_t);
char *ustrdup( const char *);
char *ustrdup2(const char *, const char *);
void ustrcat(char **p, int *siz, ...);
char *ustrprintf(const char *fmt, ...);

#ifdef __GNUC__
# define ALLOCA __builtin_alloca
#else
# define ALLOCA alloca
#endif

#endif
