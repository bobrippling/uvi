#ifndef ALLOC_H
#define ALLOC_H

enum allocfail
{
	ALLOC_UMALLOC = 2,
	ALLOC_BUFFER_C,
	ALLOC_COMMAND_C,
	ALLOC_NCURSES_1,
	ALLOC_NCURSES_2,
	ALLOC_VIEW
};

extern jmp_buf allocerr;
void *umalloc(size_t);

#endif
