#ifndef COMMAND_H
#define COMMAND_H

int runcommand(const char *,
		struct range *,
		buffer_t *,
		int *saved, int *curline,
		void (*)(void),
		void (*)(const char *)
		);

#endif
