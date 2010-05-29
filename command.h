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
	char *in,
	char *const saved, char *const readonly,
	int *const curline,
	buffer_t *const buffer,
	void      (*const wrongfunc)(void),
	void      (*const pfunc)(const char *, ...),
	enum gret (*const gfunc)(char *, int),
	int	      (*const qfunc)(const char *),
	void      (*const shellout)(const char *)
	);

buffer_t *command_readfile(const char *, char *const,
    void (*const )(const char *, ...));

void command_dumpbuffer(buffer_t *);

#endif
