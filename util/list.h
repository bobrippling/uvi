#ifndef LIST_H
#define LIST_H

struct list
{
	void *data;
	struct list *next, *prev;
} *list_new(void *);

void list_insertbefore(struct list *, void *); /* inserts char * before the list * */
void list_insertafter(struct list *, void *);	/* inserts char * after the list * */
void list_append(struct list *, void *); /* inserts char * at the very end of the list */

void list_insertlistbefore(struct list *, struct list *);
void list_insertlistafter(struct list *, struct list *);
void list_appendlist(struct list *, struct list *);

void				*list_extract(struct list *); /* removes the list * from its list and returns it */
void				 list_remove(struct list *); /* as above, but frees the extract'd */
struct list *list_extract_range(struct list **, int);
void         list_remove_range(struct list **, int, void (*)(void *));

/*
 * list_extract returns the extracted list, and adjusts the pointer
 * passed to it, so that it points to the top of the new list
 */

int list_count(struct list *);
int list_indexof(struct list *, struct list *);

struct list *list_gethead(struct list *);
struct list *list_gettail(struct list *);
struct list *list_getindex(struct list *, int);

struct list *list_copy_range(struct list *l, void *(*f_dup)(void *), struct range *r);
struct list *list_copy(      struct list *l, void *(*f_dup)(void *));

void list_free(struct list *l, void (*f)(void *));

struct list *list_from_fd(      int  fd,       int *haseol);
struct list *list_from_file(    FILE *f,       int *haseol);
struct list *list_from_filename(const char *s, int *haseol);

#endif
