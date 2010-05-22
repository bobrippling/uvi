#include <string.h>

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "buffer.h"
#include "list.h"

int runcommand(const char *s,
							struct range *rng,
							buffer_t *buffer,
							int *saved, int *curline,
							void (*wrongfunc)(void),
							void (*pfunc)(const char *))
{
	switch(*s){
		case '\0':
			wrongfunc();
			break;

		case 'q':
			if(rng || strlen(s) > 2)
				wrongfunc();
			else{
				switch(s[1]){
					case '\0':
						if(!*saved){
							pfunc("unsaved");
							break;
						}else
					case '!':
						return 0;

					default:
						wrongfunc();
				}
			}
			break;

		case 'p':
			if(strlen(s) == 1){
				struct list *l;
				if(rng){
					int i = rng->start - 1;

					for(l = list_getindex(buffer->lines, i);
							i++ != rng->end;
							l = l->next)
						pfunc(l->data);

				}else{
					l = buffer->lines;
					if(l->data)
						while(l){
							pfunc(l->data);
							l = l->next;
						}
				}
			}else
				wrongfunc();
			break;

		case 'd':
			if(strlen(s) == 1){
				struct list *l;
				if(rng){
					l = list_getindex(buffer->lines, rng->start - 1);

					list_remove_range(&l, rng->end - rng->start + 1);

					buffer->lines = list_gethead(l);
				}else
					list_remove(list_getindex(buffer->lines, *curline - 1));

				*saved = 0;
			}else
				wrongfunc();
			break;

		default:
			wrongfunc();
	}

	return 1;
}
