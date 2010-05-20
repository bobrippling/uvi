#include <stdlib.h> /* free() only */
#include <setjmp.h> /* alloc.h */

#include "list.h"
#include "alloc.h"

struct list *list_new(char *d)
{
	struct list *l = umalloc(sizeof(*l));
	l->data = d;
	l->next = l->prev = NULL;
	return l;
}

/* inserts char * before the list * */
void list_insertbefore(struct list *l, char *d)
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

void list_insertafter(struct list *l, char *d)
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
void list_append(struct list *l, char *d)
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

/* removes the list * from its list and returns it */
char *list_extract(struct list *l)
{
	char *ret = l->data;

	if(l->next){
		struct list *delthis = l->next;
		l->data = l->next->data;
		l->next = l->next->next;

		free(delthis);
	}else if(l->prev){
		l->prev->next = NULL;
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
	if(i > 0)
		return NULL;
	return l;
}

void list_free(struct list *l)
{
	struct list *del;

	l = list_gethead(l);

	while(l){
		del = l;
		l = l->next;
		free(del->data);
		free(del);
	}
}
