#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/uio.h>
#include <setjmp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "util/list.h"
#include "util/io.h"

buffer_t *buffer_new_list(struct list *l)
{
	buffer_t *b = umalloc(sizeof(*b));
	memset(b, '\0', sizeof(*b));

	b->lines = l;

	b->nlines = b->eol = 1;
	b->opentime = time(NULL);

	return b;
}

buffer_t *buffer_new(char *p)
{
	return buffer_new_list(list_new(p));
}

buffer_t *buffer_new_empty()
{
	char *s = umalloc(sizeof(char));
	*s = '\0';

	return buffer_new(s);
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

int buffer_read(buffer_t **buffer, FILE *f)
{
	struct list *l;
	buffer_t *b;
	int eol;

	l = list_from_file(f, &eol);
	if(!l)
		return -1;

	b = buffer_new_list(l);

	b->eol = eol;
	b->nlines = list_count(b->lines);
	b->modified = !b->eol;

	b->dirty = 1;
	/* this is an internal line-change memory (not to do with saving) */

	if(b->nlines == 0){
		/* create a line + set modified */
		char *s = umalloc(sizeof(char));

		*s = '\0';
		list_append(b->lines, s);

		b->modified = 1;
	}

	*buffer = b;

	return buffer_nchars(b);
#undef b
}

/* returns bytes written */
int buffer_write(buffer_t *b)
{
#define BUFFER_USE_WRITEV 0 /* XXX */
#if BUFFER_USE_WRITEV
# define MAX_BYTES 128 /* 2 ^ (sizeof(ssize_t)-1) */
	FILE *f = fopen(b->fname, "w");
	struct iovec *iov, *iovtmp;
	struct list *l = list_gethead(b->lines);
	int count, nwrite = 0, eno, totalsize = 0, fno;
	/*can't be const*/char nl = '\n';

	if(!f)
		return -1;

	count = list_count(l) * 2;
	iovtmp = iov = umalloc(count * sizeof(*iov));
	/*
	 * allocate too many if it's eol. doesn't matter, since count is dealt with
	 * later
	 */

	while(l){
		iovtmp->iov_len = strlen((iovtmp->iov_base = l->data));
		iovtmp++;

		iovtmp->iov_base = &nl;
		iovtmp++;

		totalsize += iov[-2].iov_len + (iovtmp[-1].iov_len  = sizeof(char));

		l = l->next;
	}

	if(!b->eol)
		count--; /* don't write the last '\n' */


	/*
	 * writev is limited by MAX_BYTES
	 * so do it in blocks of that size
	 */
	nwrite = totalsize / MAX_BYTES;
	fno = fileno(f);
	errno = 0;
	fprintf(stderr, "need to do at least %d writes for %d bytes (MAX_BYTES = %d)\n",
			nwrite, totalsize, MAX_BYTES);

	while(nwrite){
		nwrite += writev(fno, iov, count);
		if(errno)
			goto bail;
		iov += nwrite;
	}
	if(totalsize % MAX_BYTES){
		fprintf(stderr, "doing an extra write of size %d\n", totalsize % MAX_BYTES);
		nwrite += writev(fno, iov, count - totalsize % MAX_BYTES);
		if(errno)
			goto bail;
	}

bail:
	eno = errno;
	free(iov);
	if(!fclose(f))
		nwrite = -1;
	else
		errno = eno;

	return nwrite;
#undef MAX_BYTES
#else
	FILE *f = fopen(b->fname, "w");
	struct list *l = list_gethead(b->lines);
	int eno;
	long nwrite = 0, w;

	if(!f)
		return -1;

	while(l->next){
		if((w = fprintf(f, "%s\n", (char *)l->data)) < 0){
			nwrite = -1;
			goto bail;
		}
		nwrite += w;
		l = l->next;
	}

	/*
	 * l is now the last item in the list
	 * (possibly the first)
	 */

	w = fprintf(f, "%s%s", (char *)l->data, b->eol ? "\n" : "");
	if(w < 0)
		nwrite = -1;
	else
		nwrite += w;

	b->opentime = time(NULL);

bail:
	eno = errno;
	if(fclose(f))
		nwrite = -1;
	else
		errno = eno;

	return nwrite;
#endif
}

void buffer_free_nolist(buffer_t *b)
{
	if(b){
		free(b->fname);
		free(b);
	}
}

void buffer_free(buffer_t *b)
{
	if(b){
		list_free(b->lines, free);
		buffer_free_nolist(b);
	}
}

void buffer_replace(buffer_t *b, struct list *l)
{
	list_free(b->lines, free);
	b->lines = l;
	b->dirty = 1;
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
	if(b->dirty){
		b->nlines = list_count(b->lines);
		b->dirty = 0;
	}

	return b->nlines;
}


void buffer_remove_range(buffer_t *buffer, struct range *rng)
{
	list_free(buffer_extract_range(buffer, rng), free);
}


struct list *buffer_extract_range(buffer_t *buffer, struct range *rng)
{
	struct list *l, *newpos, *extracted;

	l = list_getindex(buffer->lines, rng->start);

	extracted = list_extract_range(&l, rng->end - rng->start + 1);
	newpos = l;

	/* l must be used below, since buffer->lines is now invalid */
	if(!(l = list_getindex(newpos, rng->start)))
		/* removed lines, and curline was in the range rng */
		l = list_gettail(newpos);

	buffer->lines = list_gethead(l);

	if(!l->data){
		/* just deleted everything, make empty line */
		l->data = umalloc(sizeof(char));
		*(char * /* smallest possible */)l->data = '\0';
	}

	buffer->dirty = 1;

	return extracted;
}

void buffer_dump(buffer_t *b, FILE *f)
{
	struct list *head;
	for(head = buffer_gethead(b); head; head = head->next)
		fprintf(f, "%s\n", (char *)head->data);
}

int buffer_external_modified(buffer_t *b)
{
	struct stat st;

	if(!buffer_hasfilename(b))
		return 0;

	if(stat(buffer_filename(b), &st))
		return 0;

	return st.st_ctime > buffer_opentime(b);
}
