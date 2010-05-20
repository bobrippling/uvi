#include <stdlib.h>
#include "list.h"

struct list *list_new(char *);

int  list_insert(struct list *, char *); /* inserts char * before the list * */
int  list_append(struct list *, char *); /* inserts char * after the list * */

struct list *list_extract(struct list *); /* removes the list * from its list and returns it */
void list_remove(struct list *); /* as above, but frees the extract'd */

void list_free(struct list *);
