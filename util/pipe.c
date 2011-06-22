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

#include "../range.h"
#include "list.h"
#include "io.h"
#include "pipe.h"
#include "alloc.h"

static int parent_write(int, struct list *);

#define WRITE_FD 1
#define READ_FD 0

struct list *pipe_read(const char *cmd)
{
	int fds[2]; /* 0 = read, 1 = write */

	if(pipe(fds) == -1)
		return NULL;

	switch(fork()){
		case -1:
			return NULL;

		case 0:
			close(fds[READ_FD]);
			if(dup2(fds[WRITE_FD], STDOUT_FILENO) == -1)
				exit(-1);

			dup2(STDOUT_FILENO, STDERR_FILENO);

			exit(system(cmd));

		default:
		{
			struct list *l;
			int saveerrno;

			close(fds[WRITE_FD]);
			l = list_from_fd(fds[READ_FD], NULL);
			saveerrno = errno;
			wait(NULL);
			errno = saveerrno;
			return l;
		}
	}
}

int pipe_write(const char *cmd, struct list *l, int close_out)
{
	int fds[2]; /* 0 = read, 1 = write */

	if(pipe(fds) == -1)
		return -1;

	switch(fork()){
		case -1:
			return -1;

		case 0:
		{
			if(close_out){
				freopen("/dev/null", "w", stdout);
				freopen("/dev/null", "w", stderr);
			}

			close(fds[WRITE_FD]);
			if(dup2(fds[READ_FD], STDIN_FILENO) == -1)
				exit(-1);

			exit(system(cmd));
		}

		default:
		{
			int ret;
			close(fds[READ_FD]);
			parent_write(fds[WRITE_FD], l);
			wait(&ret);
			return ret;
		}
	}
}

struct list *pipe_readwrite(const char *cmd, struct list *l)
{
	int parent_to_child[2], child_to_parent[2];

	if(pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1){
		close(parent_to_child[READ_FD]);
		close(parent_to_child[WRITE_FD]);
		close(child_to_parent[READ_FD]);
		close(child_to_parent[WRITE_FD]);
		return NULL;
	}

	switch(fork()){
		case -1:
			return NULL;

		case 0:
			close(parent_to_child[WRITE_FD]);
			close(child_to_parent[READ_FD]);

			if(dup2(parent_to_child[READ_FD],  STDIN_FILENO ) == -1 ||
				 dup2(child_to_parent[WRITE_FD], STDOUT_FILENO) == -1 ||
				 dup2(STDOUT_FILENO, STDERR_FILENO) == -1)

				exit(-1);

			exit(system(cmd));

		default:
		{
			struct list *ret;
			int saveerrno;

			close(parent_to_child[READ_FD]);
			close(child_to_parent[WRITE_FD]);

			if(parent_write(parent_to_child[WRITE_FD], l))
				ret = NULL;
			else
				ret = list_from_fd(child_to_parent[READ_FD], NULL);

			saveerrno = errno;
			wait(NULL);
			errno = saveerrno;

			return ret;
		}
	}
}

static int parent_write(int fd, struct list *l)
{
	static const char nl = '\n';
	int ret = 0;

	for(; l; l = l->next)
		if(write(fd, l->data, strlen(l->data)) == -1 ||
				write(fd, &nl, 1) == -1){
			ret = -1;
			break;
		}

	close(fd);

	return ret;
}
