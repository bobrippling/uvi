#ifndef LIST_H
#define LIST_H

struct list
{
	char *data;
	struct list *next, *prev;
} *list_new(char *);

void list_insertbefore(struct list *, char *); /* inserts char * before the list * */
void list_insertafter(struct list *, char *);	/* inserts char * after the list * */

void list_append(struct list *, char *); /* inserts char * at the very end of the list */

char        *list_extract(struct list *); /* removes the list * from its list and returns it */
void         list_remove(struct list *); /* as above, but frees the extract'd */
struct list *list_extract_range(struct list **, int);
void         list_remove_range(struct list **, int);

/*
 * list_extract returns the extracted list, and adjusts the pointer
 * passed to it, so that it points to the top of the new list
 */

int list_count(struct list *);

struct list *list_gethead(struct list *);
struct list *list_gettail(struct list *);
struct list *list_getindex(struct list *, int);

void list_free(struct list *);

#endif
