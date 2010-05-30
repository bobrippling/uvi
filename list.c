#include <stdlib.h> /* free() only */
#include <setjmp.h> /* alloc.h */

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

		while(bot->next && --count)
			bot = bot->next;

		if(!(*l = bot->next))
			*l = list_new(NULL);
		(*l)->prev = NULL;
		bot->next = NULL;
		return top;
	}else{
		struct list *top = (*l)->prev, *bot = *l, *ret;

		while(bot->next && --count)
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

void list_remove_range(struct list **l, int count)
{
	list_free(list_extract_range(l, count));
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
