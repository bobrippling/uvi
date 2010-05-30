#define _POSIX_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/uio.h>
#include <setjmp.h>
#include <string.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "util/list.h"

static int fgetline(char **, FILE *, char *);

buffer_t *buffer_new(char *p)
{
	buffer_t *b = umalloc(sizeof(*b));
	b->lines = list_new(p);
	b->fname = NULL;
	b->haseol = 1;
	b->changed = 0;
	b->nlines = 1;
	return b;
}

void buffer_setfilename(buffer_t *b, const char *s)
{
	free(b->fname);
	if(s){
		b->fname = umalloc(strlen(s) + 1);
		strcpy(b->fname, s);
	}else
		b->fname = NULL;
}

int buffer_read(buffer_t **buffer, const char *fname)
{
	FILE *f = fopen(fname, "r");
	struct list *tmp;
	int nread = 0, nlines = 0;
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
		nlines++;

		list_append(tmp, s);
		tmp = list_gettail(tmp);
	}while(1);


	(*buffer)->fname = umalloc(1+strlen(fname));
	strcpy((*buffer)->fname, fname);

	(*buffer)->haseol = haseol;
	(*buffer)->changed = 0;
	/* this is an internal line-change memory (not to do with saving) */

	(*buffer)->nlines  = nlines;

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
	struct list *l = list_gethead(b->lines);
	int count, nwrite, eno;
	/*can't be const*/char nl = '\n';

	if(!f)
		return -1;

	iovtmp = iov = umalloc((count = list_count(l) * 2) * sizeof(*iov));

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
		return 1;

	while(l){
		chars += 1 + strlen(l->data);
		l = l->next;
	}

	return chars;
}

int buffer_nlines(buffer_t *b)
{
	if(buffer_changed(b)){
		b->nlines = list_count(b->lines);
		buffer_changed(b) = 0;
	}

	return b->nlines;
}


void buffer_remove_range(buffer_t *buffer, struct range *rng)
{
	list_free(buffer_extract_range(buffer, rng));
}


struct list *buffer_extract_range(buffer_t *buffer, struct range *rng)
{
	struct list *l, *newpos, *extracted;

	l = list_getindex(buffer->lines, rng->start - 1);

	extracted = list_extract_range(&l, rng->end - rng->start + 1);
	newpos = l;

	/* l must be used below, since buffer->lines is now invalid */
	if(!(l = list_getindex(newpos, rng->start - 1)))
		/* removed lines, and curline was in the range rng */
		l = list_gettail(newpos);

	buffer->lines = list_gethead(l);

	if(!l->data){
		/* just deleted everything, make empty line */
		l->data = umalloc(sizeof(char));
		*(char * /* smallest possible */)l->data = '\0';
	}

	buffer->changed = 1;

	return extracted;
}
