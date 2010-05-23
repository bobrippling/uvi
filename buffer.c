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

static int fgetline(char **, FILE *, char *);

buffer_t *buffer_new(char *p)
{
	buffer_t *b = umalloc(sizeof(*b));
	b->lines = list_new(p);
	b->fname = NULL;
	return b;
}

int buffer_read(buffer_t **buffer, const char *fname)
{
	FILE *f = fopen(fname, "r");
	struct list *tmp;
	int nread = 0;
	char *s, haseol;

	if(!f)
		return -1;

	*buffer = buffer_new(NULL);
	tmp = (*buffer)->lines;

	do{
		int thisread;

		if((thisread = fgetline(&s, f, &haseol)) < 0){
			int eno = errno;
			buffer_free(*buffer);
			fclose(f);
			errno = eno;
			return -1;
		}else if(thisread == 0){
			/* assert(feof()) */
			fclose(f);
			break;
		}

		nread += thisread;

		list_append(tmp, s);
		tmp = list_gettail(tmp);
	}while(1);


	(*buffer)->fname = umalloc(1+strlen(fname));
	strcpy((*buffer)->fname, fname);

	(*buffer)->haseol = haseol;

	return nread;
}

static int fgetline(char **s, FILE *in, char *haseol)
{
#define BUFFER_SIZE 128
/* nearest 2^n, st > 80 */
	size_t buffer_size = BUFFER_SIZE;
	char *c;
	const long offset = ftell(in);

	*s = umalloc(buffer_size);

	do{
		if(!fgets(*s, buffer_size, in)){
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
			*haseol = 1;
			return 1 + strlen(*s);
		}else if(feof(in)){
			/* no eol... jerks */
			*haseol = 0;
			return strlen(*s);
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
#undef BUFFER_SIZE
}


/* returns bytes written */
int buffer_write(buffer_t *b)
{
	FILE *f = fopen(b->fname, "w");
	struct iovec *iov, *iovtmp;
	struct list *l = b->lines;
	int count, nwrite, eno;
	/*can't be const*/char nl = '\n';

	if(!f)
		return -1;

	iovtmp = iov = umalloc((count = list_count(b->lines) * 2) * sizeof(*iov));

	while(l){
		iovtmp->iov_len	= strlen((iovtmp->iov_base = l->data));
		iovtmp++;
		iovtmp->iov_base = &nl;
		iovtmp->iov_len	= 1;
		iovtmp++;
		l = l->next;
	}

	nwrite = writev(fileno(f), iov, count);
	eno = errno;
	free(iov);
	fclose(f);
	errno = eno;

	return nwrite;
}

void buffer_free(buffer_t *b)
{
	list_free(b->lines);
	free(b->fname);
	free(b);
}

int buffer_nchars(buffer_t *b)
{
	struct list *l = list_gethead(b->lines);
	int chars = 0;

	if(!l->data)
		return 0;

	while(l){
		chars += 1 + strlen(l->data);
		l = l->next;
	}

	return chars;
}

int buffer_nlines(buffer_t *b)
{
	return list_count(b->lines);
}
