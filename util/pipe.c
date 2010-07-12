#define _POSIX_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "io.h"
#include "pipe.h"

struct list *pipe_read(const char *cmd)
{
	struct list *l = list_new(NULL);
	FILE *f;
	int fds[2]; /* 0 = read, 1 = write */

	if(pipe(fds) == -1)
		return NULL;

	switch(fork()){
		case -1:
			return NULL;

		case 0:
		{
			char *line = NULL, haseol = 0;
			close(fds[1]);

			f = fdopen(fds[0], "r");
			if(!f)
				return NULL;

			while(fgetline(&line, f, &haseol) == 0)
				list_append(l, line);

			fclose(f); /* close() on fds[0] */
			return list_gethead(l);
		}

		default:
			/* set STDOUT_FILENO to be a duplicate of fds[1] */
			if(dup2(STDOUT_FILENO, fds[1]) == -1)
				return NULL;

			exit(system(cmd));
	}
}

int pipe_write(const char *cmd, struct list *l)
{
	int fds[2]; /* 0 = read, 1 = write */

	if(pipe(fds) == -1)
		return -1;

	switch(fork()){
		case -1:
			return -1;

		case 0:
			if(dup2(STDIN_FILENO, fds[0]) == -1)
				exit(-1);
			exit(system(cmd));

		default:
			/* parent */
			for(; l; l = l->next)
				if(write(fds[1], l->data, strlen(l->data)) == -1)
					return -1;
			return 0;
	}
}
