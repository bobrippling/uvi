#ifndef COMMAND_H
#define COMMAND_H

int runcommand(
		const char *,
		struct range *,
		buffer_t *,
		struct list *curline,
		int *saved,
		void (*)(void),
		void (*)(const char *)
		);

#endif
