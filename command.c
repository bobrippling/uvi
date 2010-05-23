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

struct list *readlines(char *(*gfunc)(char *, int))
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
	char *in,
	buffer_t *buffer,
	int *curline, int *saved,
	void (*wrongfunc)(void),
	void (*pfunc)(const char *, ...),
	char *(*gfunc)(char *, int),
	int	(*qfunc)(const char *)
	)
{
#define HAVE_RANGE (s > in)
	struct range lim, rng;
	int flag = 0;
	char *s;

	lim.start = *curline;
	lim.end		= list_count(buffer_lines(buffer));

	s = parserange(in, &rng, &lim, qfunc, pfunc);
	/* from this point on, s/in/s/g */
	if(!s)
		return 1;
	else if(HAVE_RANGE && *s == '\0'){
		/* just a number, move to that line */
		*curline = rng.start - 1;
		return 1;
	}

	switch(*s){
		case '\0':
			wrongfunc();
			break;

		case 'a':
			flag = 1;
		case 'i':
			if(!HAVE_RANGE && strlen(s) == 1){
				struct list *l;
insert:
				l = readlines(gfunc);

				if(l){
					struct list *line = list_getindex(buffer_lines(buffer), *curline);
					if(flag)
						list_insertlistafter(line, l);
					else
						list_insertlistbefore(line, l);
					*saved = 0;
				}
			}else
				wrongfunc();
			break;

		case 'w':
			if(HAVE_RANGE || strlen(s) > 2){
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

			if(HAVE_RANGE || strlen(s) > 2)
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
			if(HAVE_RANGE)
				wrongfunc();
			else
				pfunc("%d", 1 + *curline);
			break;

		case 'p':
			if(strlen(s) == 1){
				struct list *l;

				if(HAVE_RANGE){
					int i = rng.start - 1;

					for(l = list_getindex(buffer_lines(buffer), i);
							i++ != rng.end;
							l = l->next)
						pfunc(l->data);

				}else{
					l = list_getindex(buffer_lines(buffer), *curline);
					if(l)
						pfunc(l->data);
					else
						pfunc("Invalid current line!");
				}
			}else
				wrongfunc();
			break;

		case 'c':
			flag = 1;

		case 'd':
			if(strlen(s) == 1){
				struct list *l;
				if(HAVE_RANGE){
					l = list_getindex(buffer_lines(buffer), rng.start - 1);

					list_remove_range(&l, rng.end - rng.start + 1);

					buffer_lines(buffer) = list_getindex(buffer_lines(buffer), rng.start - 1);
				}else{
					l = list_getindex(buffer_lines(buffer), *curline);
					if(l)
						list_remove(l);
					else{
						pfunc("Invalid current line!");
						break;
					}
				}

				*saved = 0;
			}else{
				wrongfunc();
				break;
			}
			if(!flag)
				break;
			/* carry on with 'c' stuff */
			flag = 0;
			goto insert;

		default:
			wrongfunc();
	}

	return 1;
}
