#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <limits.h>
#include <sys/mman.h>
#include <errno.h>

#include "../range.h"
#include "list.h"
#include "alloc.h"

struct list *list_new(void *d)
{
	struct list *l = umalloc(sizeof(*l));
	l->data = d;
	l->next = l->prev = NULL;
	return l;
}

/* inserts char * before the list * */
void list_insertbefore(struct list *l, void *d)
{
	if(l->prev)
		list_insertafter(l->prev, d);
	else if(l->data){
		/* at head: append a node, but then swap the data around */
		char *tmp;
		list_insertafter(l, d);

		tmp = l->data;
		l->data = l->next->data;
		l->next->data = tmp;
	}else
		/* l doesn't have prev, nor ->data. therefore, tis head */
		l->data = d;
}

void list_insertafter(struct list *l, void *d)
{
	if(l->data){
		struct list *next = l->next;

		l->next = list_new(d);
		l->next->prev = l;
		if((l->next->next = next))
			next->prev = l->next;
	}else
		l->data = d;
}

/* inserts char * at the very end of the list */
void list_append(struct list *l, void *d)
{
	if(!l->data){
		l->data = d;
		return;
	}

	while(l->next)
		l = l->next;

	l->next = list_new(d);
	l->next->prev = l;
}

void list_insertlistbefore(struct list *l, struct list *new)
{
	new = list_gethead(new);

	if(!l->data){
		/* empty list; trivial */
		l->data = new->data;
		new = new->next;
		free(new->prev);

		l->next /* should be null */ = new;
		new->prev = l;
	}else if(!l->prev){
		/*
		 * at head, and l->data is occupied
		 * replace l->data with new->data
		 * and l->next with new->next
		 * then tag l->data onto the end of new
		 */
		struct list *block = new;
		char *saved = l->data;

		l->data = new->data;
		block->data = saved;
		new = new->next;

		if(new){
			/* more than one line being inserted */
			struct list *newend = list_gettail(new);

			newend->next = block;
			block->prev = newend;

			newend = newend->next;

			if((newend->next = l->next))
				newend->next->prev = newend;

			new->prev = l;
			l->next = new;

		}else{
			/* tag block onto the end of l */
			if((block->next = l->next))
				block->next->prev = block;
			l->next = block;
			block->prev = l;
		}
	}else
		list_insertlistafter(l->prev, new);
}

void list_insertlistafter(struct list *l, struct list *new)
{
	struct list *newend = list_gettail(new);

	newend->next = l->next;
	if(newend->next)
		newend->next->prev = newend;

	l->next = new;
	new->prev = l;
}


void list_appendlist(struct list *l, struct list *new)
{
	l = list_gettail(l);
	if(!l->data){
		struct list *delthis = new;

		l->data = new->data;
		new = new->next;
		free(delthis);

		l->next = new;
		if(new)
			new->prev = l;
	}else{
		l->next = new;
		new->prev = l;
	}
}


/* removes the list * from its list and returns it */
void *list_extract(struct list *l)
{
	void *ret = l->data;

	if(l->next){
		struct list *delthis = l->next;
		l->data = l->next->data;
		l->next = l->next->next;
		if(l->next)
			l->next->prev = l;

		free(delthis);
	}else if(l->prev){
		l->prev->next = NULL; /* this is fine, l->next = NULL */
		free(l);
	}else
		l->data = NULL;

	return ret;
}

/* as above, but frees the extract'd */
void list_remove(struct list *l)
{
	free(list_extract(l));
}

struct list *list_extract_range(struct list **l, int count)
{
	if(!(*l)->prev){
		/* we can adjust where l points -> trivial */
		struct list *top = *l, *bot = (*l)->next;

		while(bot->next && --count > 0)
			bot = bot->next;

		if(!(*l = bot->next))
			*l = list_new(NULL);
		(*l)->prev = NULL;
		bot->next = NULL;
		return top;
	}else{
		struct list *top = (*l)->prev, *bot = *l, *ret;

		while(bot->next && --count > 0)
			bot = bot->next;

		top->next = bot->next;
		if(top->next)
			top->next->prev = top;

		/* isolate the list */
		bot->next = NULL;
		(*l)->prev = NULL;
		ret = *l;
		*l = top;

		return ret;
	}
}

void list_remove_range(struct list **l, int count, void (*f)(void *))
{
	list_free(list_extract_range(l, count), f);
}

int list_count(struct list *l)
{
	int i;
	l = list_gethead(l);

	if(!l->next && !l->data)
		return 0;

	i = 1;
	while(l->next)
		l = l->next, ++i;
	return i;
}

int list_indexof(struct list *l, struct list *p)
{
	int i = 0;
	l = list_gethead(l);
	while(l && l != p)
		i++, l = l->next;

	if(l == p)
		return i;
	return -1;
}

struct list *list_gethead(struct list *l)
{
	while(l->prev)
		l = l->prev;
	return l;
}

struct list *list_gettail(struct list *l)
{
	while(l->next)
		l = l->next;
	return l;
}

struct list *list_getindex(struct list *l, int i)
{
	l = list_gethead(l);
	while(i --> 0 && l->next)
		l = l->next;

	if(i != -1)
		return NULL;

	return l;
}

void list_free_nodata(struct list *l)
{
	struct list *del;
	l = list_gethead(l);
	while(l){
		del = l;
		l = l->next;
		free(del);
	}
}

void list_free(struct list *l, void (*f)(void *))
{
	struct list *del;

	l = list_gethead(l);

	while(l){
		del = l;
		l = l->next;
		if(del->data)
			f(del->data);
		free(del);
	}
}

struct list *list_copy_range(struct list *l, void *(*f_dup)(void *), struct range *r)
{
	struct list *iter;
	struct list *new;
	int i;

	new = list_new(NULL);

	iter = list_getindex(l, r->start);

	for(i = r->start; iter && i <= r->end; i++, iter = iter->next)
		list_append(new, f_dup(iter->data));

	return new;
}

struct list *list_copy(struct list *l, void *(*f_dup)(void *))
{
	struct range r;

	r.start = 0;
	r.end = INT_MAX;

	return list_copy_range(l, f_dup, &r);
}

static struct list *mmap_to_lines(char *mem, int *haseol, size_t len)
{
	struct list *head = list_new(NULL), *cur = head;
	char *last = mem + len;
	char *a, *b;
	int eol;

	for(a = b = mem; a < last; a++)
		if(*a == '\n'){ /* TODO: memchr() */
			char *data = umalloc(a - b + 1);

			memcpy(data, b, a - b);

			data[a - b] = '\0';

			list_append(cur, data);
			cur = list_gettail(cur);
			b = a + 1;
		}

	if(!(eol = a == b)){
		char *rest = umalloc(a - b + 1);
		memcpy(rest, b, a - b);
		rest[a - b] = '\0';
		list_append(cur, rest);
	}

	if(haseol)
		*haseol = eol;

	return head;
}

struct list *list_from_fd(int fd, int *haseol)
{
	struct stat st;
	struct list *l;
	void *mem;

	if(fstat(fd, &st) == -1)
		return NULL;

	if(st.st_size == 0)
		goto fallback; /* could be stdin */

	mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if(mem == MAP_FAILED){
		if(errno == EINVAL){
			/* fallback to fread - probably stdin */
			char buffer[1024];
			struct list *l;
			struct list *i;
			FILE *f;
			int eol = 1;

fallback:
			f = fdopen(fd, "r");
			if(!f)
				return NULL;

			i = l = list_new(NULL);

			while(fgets(buffer, sizeof buffer, f)){
				char *nl = strchr(buffer, '\n');
				if(nl){
					*nl = '\0';
					eol = 1;
				}else{
					eol = 0;
				}
				list_append(i, ustrdup(buffer));
				i = list_gettail(i);
			}

			if(haseol)
				*haseol = eol;

			if(ferror(f)){
				list_free(l, free);
				l = NULL;
			}
			fclose(f);
			return l;
		}

		return NULL;
	}

	l = mmap_to_lines(mem, haseol, st.st_size);
	munmap(mem, st.st_size);

	return l;
}

struct list *list_from_file(FILE *f, int *haseol)
{
	int fd = fileno(f);
	if(fd == -1)
		return NULL;
	return list_from_fd(fd, haseol);
}

struct list *list_from_filename(const char *s, int *haseol)
{
	struct list *l;
	FILE *f;

	f = fopen(s, "r");
	if(!f)
		return NULL;

	l = list_from_file(f, haseol);
	fclose(f);

	return l;
}
