#define _POSIX_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/uio.h>
#include <setjmp.h>
#include <string.h>
/* stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
/* getpwuid */
#include <pwd.h>

#include "util/alloc.h"
#include "range.h"
#include "buffer.h"
#include "util/list.h"

#include "config.h"

extern struct settings global_settings;

static int fgetline(char **, FILE *, char *);
static char canwrite(mode_t, uid_t, gid_t);

buffer_t *buffer_new(char *p)
{
	buffer_t *b = umalloc(sizeof(*b));
	memset(b, '\0', sizeof(*b));

	b->lines = list_new(p);
	b->nlines = b->eol = 1;

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
#define b (*buffer)
	FILE *f = fopen(fname, "r");
	struct list *tmp;
	struct stat st;
	int nread = 0, nlines = 0;
	char *s, haseol = 1;

	if(!f)
		return -1;

	b = buffer_new(NULL);
	tmp = b->lines;

	do{
		int thisread;

		if((thisread = fgetline(&s, f, &haseol)) < 0){
			int eno = errno;

			buffer_free(b);
			b = NULL;

			if(fclose(f) /* error */ && !eno)
				eno = errno;

			errno = eno;
			return -1;
		}else if(thisread == 0){
			/* assert(feof()) */
			if(fclose(f)){
				int eno = errno;

				buffer_free(b);
				b = NULL;

				errno = eno;
				return -1;
			}

			break;
		}

		nread += thisread;
		nlines++;

		list_append(tmp, s);
		tmp = list_gettail(tmp);
	}while(1);


	b->fname = umalloc(1+strlen(fname));
	strcpy(b->fname, fname);

	b->modified = !(b->eol = haseol);

	b->changed = 1;
	/* this is an internal line-change memory (not to do with saving) */

	if(nread == 0){
		/* create a line + set modified */
		b->modified = 1;
		s = umalloc(sizeof(char));
		*s = '\0';
		list_append(b->lines, s);
	}

	if(!stat(fname, &st))
		b->readonly = !canwrite(st.st_mode, st.st_uid, st.st_gid);
	else
		b->readonly = 0;

	b->nlines  = nlines;

	return nread;
#undef b
}

static char canwrite(mode_t mode, uid_t uid, gid_t gid)
{
	struct passwd *pd;

	if(mode & 02)
		return 1;

	pd = getpwuid(getuid());
	if(!pd)
		return 0;

	if((mode & 020) && gid == pd->pw_gid)
		return 1;

	if((mode & 0200) && uid == pd->pw_uid)
		return 1;

	return 0;
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
				longjmp(allocerr, ALLOC_BUFFER_C);
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

bail:
	eno = errno;
	if(fclose(f))
		nwrite = -1;
	else
		errno = eno;

	return nwrite;
#endif
}

void buffer_free(buffer_t *b)
{
	if(b){
		list_free(b->lines);
		free(b->fname);
		free(b);
	}
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
