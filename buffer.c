#define _POSIX_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/uio.h>
#include <setjmp.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "list.h"

#define BUFFER_SIZE 128
/* nearest 2^n, st > 80 */

static int fgetline(char **, FILE *);

buffer_t *buffer_new()
{
	buffer_t *b = umalloc(sizeof(*b));
	b->lines = list_new(NULL);
	b->fname = NULL;
	return b;
}

int buffer_read(buffer_t *buffer, const char *fname)
{
	FILE *f = fopen(fname, "r");
	struct list *tmp;
	int nread = 0;
	char *s;

	if(!f)
		return -1;

	buffer = buffer_new();
	tmp = buffer->lines;

	do{
		int thisread;

		if((thisread = fgetline(&s, f)) < 0){
			int eno = errno;
			buffer_free(buffer);
			errno = eno;
			return -1;
		}else if(thisread == 0)
			/* assert(feof()) */
			break;

		nread += thisread;

		list_append(tmp, s);
		tmp = list_gettail(tmp);
	}while(1);

	buffer->fname = umalloc(1+strlen(fname));
	strcpy(buffer->fname, fname);

	return nread;
}

static int fgetline(char **s, FILE *in)
{
	int buffer_size = BUFFER_SIZE, nread;
	char *c;
	const long offset = ftell(in);

	*s = umalloc(buffer_size);

	do{
		if((nread = fread(*s, buffer_size, sizeof(char), in))){
			int eno = errno;
			if(feof(in)){
				/* no chars read */
				free(*s);
				return 0;
			}

			free(*s);
			*s = NULL;
			errno = eno;
			return -1;
		}

		if((c = strchr(*s, '\n'))){
			/* in an ideal world... */
			*c = '\0';
			return nread;
		}else{
			/* need a bigger biffer for this line */
			/* and buffer, too */
			char *tmp;
			buffer_size *= 2;

			tmp = realloc(*s, buffer_size);
			if(!tmp){
				free(*s);
				longjmp(allocerr, 1);
			}
			*s = tmp;
			if(fseek(in, offset, SEEK_SET) == -1){
				int eno = errno;
				free(*s);
				*s = NULL;
				errno = eno;
				return -1;
			}
		}
	}while(1);
}


/* returns bytes written */
int buffer_write(buffer_t *b, const char *fname)
{
	FILE *f = fopen(fname, "w");
	struct iovec *iov, *iovtmp;
	struct list *l = b->lines;
	int count, nwrite;

	if(!f)
		return 0;

	iovtmp = iov = umalloc((count = list_count(b->lines)) * sizeof(*iov));

	while(l){
		iovtmp->iov_len  = strlen((iovtmp->iov_base = l->data));
		iovtmp++;
		l = l->next;
	}

	nwrite = writev(fileno(f), iov, count);
	free(iov);
	return nwrite;
}

void buffer_free(buffer_t *b)
{
	list_free(b->lines);
	free(b->fname);
	free(b);
}
