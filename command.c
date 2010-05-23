#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "buffer.h"
#include "list.h"
#include "alloc.h"

static struct list *readlines(char *(*gfunc)(char *, int));

static struct list *readlines(char *(*gfunc)(char *, int))
{
#define BUFFER_SIZE 128
	struct list *l = list_new(NULL);
	char buffer[BUFFER_SIZE];

	while(gfunc(buffer, BUFFER_SIZE)){
		char *nl = strchr(buffer, '\n');

		if(nl){
			char *s = umalloc(strlen(buffer)); /* no need for +1 - \n is removed */
			*nl = '\0';
			strcpy(s, buffer);
			list_append(l, s);
		}else{
			int size = BUFFER_SIZE;
			char *s = NULL, *insert;

			do{
				char *tmp;

				size *= 2;
				if(!(tmp = realloc(s, size))){
					free(s);
					longjmp(allocerr, 1);
				}
				s = tmp;
				insert = s + size - BUFFER_SIZE;

				strcpy(insert, buffer);
				if((tmp = strchr(insert, '\n'))){
					*tmp = '\0';
					break;
				}
			}while(gfunc(buffer, BUFFER_SIZE));
		}
	}

	if(!l->data){
		/* EOF straight away */
		l->data = umalloc(sizeof(char));
		*l->data = '\0';
	}

	return l;
}


int runcommand(
	const char *s,
	struct range *rng,
	buffer_t *buffer,
	struct list *curline,
	int *saved,
	void (*wrongfunc)(void),
	void (*pfunc)(const char *, ...),
	char *(*gfunc)(char *, int))
{
	int flag = 0;

	switch(*s){
		case '\0':
			wrongfunc();
			break;

		case 'a':
			flag = 1;
		case 'i':
			if(!rng && strlen(s) == 1){
				struct list *l = readlines(gfunc), *tmp;

				if(l){
					void (*func)(struct list *, char *);

					l = list_gettail(l);
					func = flag ? &list_insertafter : &list_insertbefore;

					if(!curline) /* empty buffer */
						curline = buffer->lines;

					while(l){
						func(curline, l->data);
						tmp = l;
						l = l->prev;
						free(tmp); /* can't free(l->next) - l may be NULL */
					}
				}
			}else
				wrongfunc();
			break;

		case 'w':
			if(rng || strlen(s) > 2){
				wrongfunc();
				break;
			}
			switch(s[1]){
				case 'q':
					flag = 1;
				case '\0':
					break;

				default:
					wrongfunc();
			}


			if(!buffer->fname){
				pfunc("buffer has no filename");
				break;
			}else{
				int nw = buffer_write(buffer);
				if(nw == -1){
					pfunc("%s: %s", buffer->fname, strerror(errno));
					break;
				}
				*saved = 1;
				pfunc("%s: %dC", buffer->fname, nw);
			}
			if(!flag)
				break;

		case 'q':
			if(flag)
				return 0;

			if(rng ||	strlen(s) > 2)
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

		case 'g':
			/* print current line index */
			if(strlen(s) == 1 && !rng){
				int i = list_indexof(buffer->lines, curline);
				if(i != -1)
					pfunc("%d", i);
				else
					pfunc("Invalid index...?");
			}else
				wrongfunc();
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

				}else if(curline)
					pfunc(curline->data);
				else
					pfunc("Invalid current line!");
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
					list_remove(curline);

				*saved = 0;
			}else
				wrongfunc();
			break;

		default:
			wrongfunc();
	}

	return 1;
}
