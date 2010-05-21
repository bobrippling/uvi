#include <stdio.h>
#include <setjmp.h>
#include <string.h>

extern char *strdup(const char *);

#include "list.h"

jmp_buf allocerr;

void printlist(struct list *);

void printlist(struct list *l)
{
	int i;
	struct list *tmp = list_gettail(l);
	if(!l->data){
		puts("(empty list)");
		return;
	}

	puts("----------");
	for(i = 0; l; l = l->next)
		printf("%d: \"%s\"\n", i++, l->data);
	puts("-----");
	for(l = tmp, i = list_count(l)-1; l; l = l->prev)
		printf("%d: \"%s\"\n", i--, l->data);
	puts("----------");
}

int main(void)
{
	struct list *l = list_new(strdup("hi there")), *tmp, *ret;
	int i, count;
	char buf[256];


	for(i = 0; i < 10; i++){
		sprintf(buf, "%d", i);
		list_append(l, strdup(buf));
	}

	printlist(l);

	tmp = l->next->next->next; count = 3;
	/*tmp = list_gettail(l); count = 1;*/
	/*tmp = list_gethead(l); count = list_count(l);*/

	printf("removing from \"%s\", count = %d\n", tmp->data, count);

	ret = list_extract_range(&tmp, count);
	l = list_gethead(tmp);

	puts("list now:");
	printlist(l);
	puts("extract'd:");
	printlist(ret);

	list_free(ret);
	list_free(l);

	return 0;
}
