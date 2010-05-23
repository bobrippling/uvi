#ifndef COMMAND_H
#define COMMAND_H

enum gret
{
	g_LAST,
	g_EOF,
	g_CONTINUE
};

struct list *readlines(enum gret (*)(char *, int));

int runcommand(
	char *,
	buffer_t *,
	int *, int *,
	/* wrongfunc, pfunc, gfunc, qfunc */
	void	    (*)(void),
	void	    (*)(const char *, ...),
	enum gret (*)(char *, int),
	int	      (*)(const char *)
	);

#endif
