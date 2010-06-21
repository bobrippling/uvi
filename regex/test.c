#include <stdio.h>

#include "regex.h"

int main(void)
{
	const char *regex = "he?ll*o the+re";
	regex_t *reg, *r;
	int indent = 0, i;

#define INDENT() for(i = 0; i < indent; i++) putchar(' ');

	r = reg = regex_create(regex);

	while(r){
		INDENT();
		printf("r->str: %s\n", r->str);
		INDENT();
		printf("r->noderepeat: %s\n", r->noderepeat == NONE ? "none" : r->noderepeat == STAR ? "star" : r->noderepeat == QMARK ? "qmark" : "plus");
		INDENT();
		printf("r->next:\n");
		indent++;

		r = r->next;
	}

	regex_free(reg);
	return 0;
}
