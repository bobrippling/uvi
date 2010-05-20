#ifndef ALLOC_H
#define ALLOC_H

extern jmp_buf allocerr;
void *umalloc(size_t);

#endif
