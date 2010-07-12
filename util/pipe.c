#define _POSIX_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <errno.h>

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "list.h"
#include "io.h"
#include "pipe.h"
#include "alloc.h"

struct list *pipe_read(const char *cmd)
{
	int fds[2]; /* 0 = read, 1 = write */

	if(pipe(fds) == -1)
		return NULL;

	switch(fork()){
		case -1:
			return NULL;

		case 0:
		{
			int devnull = open("/dev/null", O_WRONLY);

			dup2(devnull, STDERR_FILENO);

			close(fds[0]);
			if(dup2(fds[1], STDOUT_FILENO) == -1)
				exit(-1);
			exit(system(cmd));
		}

		default:
		{
			char *line = NULL;
			int ret, saveerrno;
			char unused;
			struct list *l = list_new(NULL);
			FILE *f = fdopen(fds[0], "r");
			saveerrno = errno;

			close(fds[1]);

			if(!f){
				errno = saveerrno;
				return NULL;
			}

			while((ret = fgetline(&line, f, &unused)) > 0)
				list_append(l, line);

			saveerrno = errno;

			/*close(fds[0]);*/
			fclose(f);

			if(ret == -1){
				list_free(l);
				errno = saveerrno;
				return NULL;
			}

			return list_gethead(l);
		}
	}
}

int pipe_write(const char *cmd, struct list *l)
{
	static const char nl = '\n';
	int fds[2]; /* 0 = read, 1 = write */

	if(pipe(fds) == -1)
		return -1;

	switch(fork()){
		case -1:
			return -1;

		case 0:
		{
			int devnull = open("/dev/null", O_WRONLY);

			dup2(devnull, STDERR_FILENO);

			close(fds[1]);
			if(dup2(fds[0], STDIN_FILENO) == -1)
				exit(-1);

			exit(system(cmd));
		}

		default:
		{
			int ret = 0;

			close(fds[0]);
			for(; l; l = l->next)
				if(write(fds[1], l->data, strlen(l->data)) == -1 ||
						write(fds[1], &nl, 1) == -1){
					ret = -1;
					break;
				}

			close(fds[1]);
			wait(NULL);
			return ret;
		}
	}
}
