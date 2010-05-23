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

struct list *command_readlines(enum gret (*gfunc)(char *, int))
{
#define BUFFER_SIZE 128
	struct list *l = list_new(NULL);
	char buffer[BUFFER_SIZE];
	int loop = 1;

	do
		switch(gfunc(buffer, BUFFER_SIZE)){
			case g_EOF:
				loop = 0;
				break;

			case g_LAST:
				/* add this buffer, then bail out */
				loop = 0;
			case g_CONTINUE:
			{
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
					}while(gfunc(buffer, BUFFER_SIZE) == g_CONTINUE);
					break;
				}
			}
		}
	while(loop);

	if(!l->data){
		/* EOF straight away */
		l->data = umalloc(sizeof(char));
		*l->data = '\0';
	}

	return l;
}


int command_run(
	char *in,
	buffer_t *buffer,
	int *curline, int *saved,
	/* Note: curline is the index, i.e. 0 to $-1 */
	void (*wrongfunc)(void),
	void (*pfunc)(const char *, ...),
	enum gret (*gfunc)(char *, int),
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
		*curline = rng.start - 1; /* -1, because they enter between 1 & $ */
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
				l = command_readlines(gfunc);

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
						pfunc("%s", l->data);

				}else{
					l = list_getindex(buffer_lines(buffer), *curline);
					if(l)
						pfunc("%s", l->data);
					else
						pfunc("Invalid current line ('p')!");
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

					/* l must be used below, since buffer_lines(buffer) is now invalid */
					l = buffer_lines(buffer) = list_getindex(l, rng.start - 1);

					if(!l->data){
						/* just deleted everything, make empty line */
						l->data = umalloc(sizeof(char));
						*l->data = '\0';
					}
				}else{
					l = list_getindex(buffer_lines(buffer), *curline);
					if(l)
						list_remove(l);
					else{
						pfunc("Invalid current line ('%c')!", *s);
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

buffer_t *command_readfile(const char *filename, void (*pfunc)(const char *, ...))
{
  buffer_t *buffer;
	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			if(errno == ENOENT)
				goto new_file;
			return NULL;
		}else if(nread == 0)
			pfunc("(empty file)");
		else
			pfunc("%s: %dC, %dL%s", filename,
					buffer_nchars(buffer), buffer_nlines(buffer),
					buffer->haseol ? "" : " (noeol)");
	}else{
    char *s;
new_file:
    s = umalloc(sizeof(char));
    *s = '\0';
		buffer = buffer_new(s);
		pfunc("(new file)");
	}
  return buffer;
}
