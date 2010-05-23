#ifndef COMMAND_H
#define COMMAND_H

int runcommand(
	char *,
	buffer_t *,
	int *, int *,
	void	(*)(void),
	void	(*)(const char *, ...),
	char *(*)(char *, int),
	int	 (*)(const char *)
	);

#endif
