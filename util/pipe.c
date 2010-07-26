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

static int parent_write(int, struct list *);
static struct list *parent_read(int);

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
			l = parent_read(fds[READ_FD]);
			saveerrno = errno;
			wait(NULL);
			errno = saveerrno;
			return l;
		}
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
		{
			int devnull = open("/dev/null", O_WRONLY);
			dup2(devnull, STDOUT_FILENO);
			dup2(devnull, STDERR_FILENO);

			close(fds[WRITE_FD]);
			if(dup2(fds[READ_FD], STDIN_FILENO) == -1)
				exit(-1);

			exit(system(cmd));
		}

		default:
		{
			int ret, saveerrno;
			close(fds[READ_FD]);
			ret = parent_write(fds[WRITE_FD], l);
			saveerrno = errno;
			wait(NULL);
			errno = saveerrno;
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
				ret = parent_read(child_to_parent[READ_FD]);

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

static struct list *parent_read(int fd)
{
	char *line = NULL;
	int ret, saveerrno;
	char unused;
	struct list *l = list_new(NULL);
	FILE *f = fdopen(fd, "r");
	saveerrno = errno;

	if(!f){
		errno = saveerrno;
		return NULL;
	}

	while((ret = fgetline(&line, f, &unused)) > 0)
		list_append(l, line);

	saveerrno = errno;

	/*close(fds[READ_FD]);*/
	fclose(f);

	if(ret == -1){
		list_free(l);
		errno = saveerrno;
		return NULL;
	}

	return list_gethead(l);
}
