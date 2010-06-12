#ifndef COMMAND_H
#define COMMAND_H

enum gret
{
	g_LAST,
	g_EOF,
	g_CONTINUE
};

struct list *command_readlines(enum gret (*)(char *, int));

/*
 * convention for pfunc() is that if it's passed
 * a string with no '\n' in, then it will print and
 * be done. If there's a '\n' then (for ncurses)
 * it will print and go to the next line and wait to
 * be called again. It resets this after command_*
 * returns
 */

int command_run(
	char *in,
	int *const curline,
	buffer_t **buffer,
	void      (*const wrongfunc)(void),
	void      (*const pfunc)(const char *, ...),
	enum gret (*const gfunc)(char *, int),
	int       (*const qfunc)(const char *, ...),
	void      (*const shellout)(const char *)
	);

buffer_t *command_readfile(const char *filename,
		char forcereadonly, void (*const pfunc)(const char *, ...));

void command_dumpbuffer(buffer_t *);

#define READ_ONLY_ERR "read only file"

#endif
