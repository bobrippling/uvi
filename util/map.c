#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "list.h"
#include "map.h"
#include "alloc.h"

static struct pair
	{
		void *k, *v;
	} *pair_new(void);

static struct pair *map_getpair(map *, void *const);


map *map_new(void)
{
	map *m = umalloc(sizeof(*m));
	m->pairs = list_new(NULL);
	return m;
}

void map_add(map *m, void *const k, void *const v)
{
	struct pair *p = map_getpair(m, k);

	if(p){
		free(p->v);
		free(p->k);
		p->v = v;
		p->k = k;
	}else{
		p = pair_new();
		p->k = k;
		p->v = v;
		list_append(m->pairs, p);
	}
}

void *map_get(map *m, void *const k)
{
	struct pair *p = map_getpair(m, k);
	if(!p)
		return NULL;
	return p->v;
}

static struct pair *map_getpair(map *m, void *const k)
{
	struct list *l = m->pairs;

	if(!l->data)
		return NULL;

	while(l){
		struct pair *p = (struct pair *)l->data;
		if(!strcmp(p->k, k))
			return p;
		l = l->next;
	}

	return NULL;
}

void map_free(map *m)
{
	/*
	 * only free the strings, not the pair structs
	 * list_free does this
	 */
	struct list *l = m->pairs;

	if(!l->data)
		return;

	while(l){
		struct pair *p = (struct pair *)l->data;
		free(p->k);
		free(p->v);
		l = l->next;
	}

	list_free(m->pairs);
	free(m);
}

/* static */

static struct pair *pair_new()
{
	struct pair *p = umalloc(sizeof(*p));
	memset(p, '\0', sizeof(*p));
	return p;
}
