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
#include "global.h"

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

int is_crlf(buffer_t *b)
{
	struct list *l;

	for(l = b->lines; l && l->data; l = l->next){
		char *s = l->data;

		if(!*s)
			return 0;

		if(s[strlen(s)-1] != '\r'){
			if(l->next)
				return 0;
			else
				/* all crlf except last line, unless only one line */
				return !!l->prev;
		}
	}

	/* all are crlf */
	return 1;
}

int buffer_read(buffer_t **buffer, FILE *f)
{
	struct list *l;
	buffer_t *b;
	int eol;

	l = list_from_file(f, &eol);
	if(!l){
		*buffer = NULL;
		return -1;
	}

	b = buffer_new_list(l);

	b->eol = eol;

	b->dirty = 1;
	if(buffer_nlines(b) == 0){
		/* create a line */
		char *s = umalloc(sizeof(char));

		*s = '\0';
		list_append(b->lines, s);

		b->nlines = 1;
	}

	if((b->crlf = is_crlf(b))){
		struct list *l;
		for(l = b->lines; l && l->data; l = l->next){
			char *s = l->data;
			const int i = strlen(s) - 1;

			if(s[i] == '\r')
				s[i] = '\0';
		}
	}

	*buffer = b;

	return buffer_nchars(b);
#undef b
}

/* returns bytes written */
int buffer_write(buffer_t *b)
{
	return buffer_write_list(b, list_gethead(b->lines));
}

int buffer_write_list(buffer_t *b, struct list *l)
{
	const char *crlf;
	FILE *f = fopen(b->fname, "w");
	int eno;
	long nwrite = 0, w;

	if(!f)
		return -1;

	if(b->crlf)
		crlf = "\r";
	else
		crlf = "";

	while(l->next){
		if((w = fprintf(f, "%s%s\n", (char *)l->data, crlf)) < 0){
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

	w = fprintf(f, "%s%s%s", (char *)l->data, b->eol ? crlf : "", b->eol ? "\n" : "");
	if(w < 0)
		nwrite = -1;
	else
		nwrite += w;

	b->opentime = time(NULL);

	if(global_settings.fsync && (fflush(f) || fsync(fileno(f)))){
		nwrite = -1;
		goto bail;
	}

bail:
	eno = errno;
	if(fclose(f))
		nwrite = -1;
	else
		errno = eno;

	return nwrite;
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
		chars += strlen(l->data);
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

	buffer->lines = l = list_gethead(l);

	if(!l->data){
		/* just deleted everything, make empty line */
		l->data = umalloc(sizeof(char));
		*(char *)l->data = '\0';
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
