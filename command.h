#ifndef COMMAND_H
#define COMMAND_H

enum gret
{
	g_LAST,
	g_EOF,
	g_CONTINUE
};

struct list *command_readlines(enum gret (*)(char *, int));

int command_run(
	char *,
	buffer_t *,
	int *, int *,
	/* wrongfunc, pfunc, gfunc, qfunc */
	void	    (*)(void),
	void	    (*)(const char *, ...),
	enum gret (*)(char *, int),
	int	      (*)(const char *)
	);

buffer_t *command_readfile(const char *,
    void (*)(const char *, ...));

#endif
