#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "buffer.h"
#include "util/list.h"
#include "vars.h"
#include "util/alloc.h"

#include "config.h"

#define READ_ONLY_ERR "read only file"

static void parse_setget(buffer_t *, char, char *, void (*)(const char *, ...), void (*)(void));

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
		*(char *)l->data = '\0';
	}

	return l;
}


int command_run(
	char *in,
	int *curline,
	buffer_t *buffer,
	/* Note: curline is the index, i.e. 0 to $-1 */
	void (*wrongfunc)(void),
	void (*pfunc)(const char *, ...),
	enum gret (*gfunc)(char *, int),
	int	(*qfunc)(const char *),
	void (*shellout)(const char *)
	)
{
#define HAVE_RANGE (s > in)
	struct range lim, rng;
	int flag = 0;
	char *s;

	lim.start = *curline;
	lim.end		= buffer_nlines(buffer);

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
			if(buffer_readonly(buffer))
				pfunc(READ_ONLY_ERR);
			else if(!HAVE_RANGE && strlen(s) == 1){
				struct list *l;
insert:
				l = command_readlines(gfunc);

				if(l){
					struct list *line = buffer_getindex(buffer, *curline);

					if(flag)
						buffer_insertlistafter(buffer, line, l);
					else
						buffer_insertlistbefore(buffer, line, l);
					buffer_modified(buffer) = 1;
				}
			}else
				wrongfunc();
			break;

		case 'w':
		{
			int bail = 0;
			char *fname;
			if(HAVE_RANGE){
				wrongfunc();
				break;
			}else if(buffer_readonly(buffer)){
				pfunc(READ_ONLY_ERR);
				break;
			}

			switch(strlen(s)){
				case 2:
					switch(s[1]){
						case 'q':
							flag = 1;
							break;

						case ' ':
							fname = s + 1;
							goto vars_fname;
							break;

						default:
							wrongfunc();
							bail = 1;
					}
				case 1:
					break;

				default:
					switch(s[1]){
						case ' ':
							fname = s + 2;
							goto vars_fname;
						case 'q':
							flag = 1;
							fname = s + 3;
vars_fname:
							buffer_setfilename(buffer, fname);
							break;

						default:
							wrongfunc();
							bail = 1;
					}
			}
			if(bail)
				break;

			if(!buffer_hasfilename(buffer)){
				pfunc("buffer has no filename (w[q] fname)");
				break;
			}else{
				int nw = buffer_write(buffer);
				if(nw == -1){
					pfunc("Couldn't save \"%s\": %s", buffer_filename(buffer), strerror(errno));
					break;
				}
				buffer_modified(buffer) = 0;
				pfunc("\"%s\" %dL, %dC written", buffer_filename(buffer),
						buffer_nlines(buffer) - !buffer_eol(buffer), nw);
			}
			if(!flag)
				break;

			s[1] = '\0';
		}

		case 'q':
			if(flag)
				return 0;

			if(HAVE_RANGE || strlen(s) > 2)
				wrongfunc();
			else{
				switch(s[1]){
					case '\0':
						if(buffer_modified(buffer)){
							pfunc("unsaved");
							break;
						}
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
			else if(strlen(s) != 1)
				goto def;
			else
				pfunc("%d", 1 + *curline);
			break;

		case 'p':
			if(strlen(s) == 1){
				struct list *l;

				if(HAVE_RANGE){
					int i = rng.start - 1;

					for(l = buffer_getindex(buffer, i);
							i++ != rng.end;
							l = l->next)
						pfunc("%s", l->data);

				}else{
					l = buffer_getindex(buffer, *curline);
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
			if(buffer_readonly(buffer))
				pfunc(READ_ONLY_ERR);
			else if(strlen(s) == 1){
				if(HAVE_RANGE){
					buffer_remove_range(buffer, &rng);

					if(!buffer_getindex(buffer, rng.start))
						*curline = buffer_indexof(buffer, buffer_gettail(buffer));

				}else{
					struct list *l = buffer_getindex(buffer, *curline);
					if(l){
						if(!l->next)
							--*curline;

						buffer_remove(buffer, l);
					}else{
						pfunc("Invalid current line ('%c')!", *s);
						break;
					}
				}

				buffer_modified(buffer) = 1;
			}else{
				wrongfunc();
				break;
			}
			if(!flag)
				break;
			/* carry on with 'c' stuff */
			flag = 0;
			goto insert;

		case '!':
			if(HAVE_RANGE)
				wrongfunc();
			else{
				if(s[1] == '\0')
					shellout("sh");
				else
					shellout(s + 1);
			}
			break;

		default:
		def:
			/* full string cmps */
			if(!strncmp(s, "set", 3))
				parse_setget(buffer, 1, s + 3, pfunc, wrongfunc);
			else if(!strncmp(s, "get", 3))
				parse_setget(buffer, 0, s + 3, pfunc, wrongfunc);
			else
				wrongfunc();
	}

	return 1;
}

buffer_t *command_readfile(const char *filename, char forcereadonly, void (*const pfunc)(const char *, ...))
{
  buffer_t *buffer;
	if(filename){
		int nread = buffer_read(&buffer, filename);

		if(nread < 0){
			if(errno == ENOENT){
				char *s = umalloc(sizeof(char));
				*s = '\0';
				buffer = buffer_new(s);
				buffer_setfilename(buffer, filename);
			}else
				return NULL;
		}else{
			/* end up here on successful read */
			if(forcereadonly)
				buffer_readonly(buffer) = 1;

			if(nread == 0)
				pfunc("(empty file)%s", buffer_readonly(buffer) ? " [read only]" : "");
			else
				pfunc("%s%s: %dC, %dL%s", filename,
						buffer_readonly(buffer) ? " [read only]" : "",
						buffer_nchars(buffer), buffer_nlines(buffer),
						buffer_eol(buffer) ? "" : " (noeol)");
		}

	}else{
    char *s = umalloc(sizeof(char));
    *s = '\0';
		buffer = buffer_new(s);
		pfunc("(new file)");
	}
  return buffer;
}

void command_dumpbuffer(buffer_t *b)
{
#define DUMP_POSTFIX "_dump"
#define POSTFIX_LEN  6
	FILE *f;

	if(!b)
		return;

	if(!buffer_hasfilename(b))
		f = fopen(PROG_NAME DUMP_POSTFIX, "w");
	else{
		char *s = malloc(strlen(buffer_filename(b)) + POSTFIX_LEN);
		if(!s)
			f = fopen(PROG_NAME DUMP_POSTFIX, "w");
		else{
			strcpy(s, buffer_filename(b));
			strcat(s, DUMP_POSTFIX);
			f = fopen(s, "w");
		}
	}

	if(f){
		struct list *head = buffer_gethead(b);
		while(head){
			fprintf(f, "%s\n", (char *)head->data);
			head = head->next;
		}
		fclose(f);
	}
}

static void parse_setget(buffer_t *b, char isset, /* is this "set" or "get"? */
		char *s, void (*pfunc)(const char *, ...), void (*wrongfunc)(void))
{
	if(*s == ' '){
		if(isalpha(*++s)){
			char *wordstart = s;

			while(isalpha(*++s));

			switch(*s){
				case '\0':
					if(isset){
						char bool;
						if(!strncmp(wordstart, "no", 2)){
							bool = 0;
							wordstart += 2;
						}else
							bool = 1;

						if(!vars_set(b, wordstart, bool))
							pfunc("\"%s\" is not a valid variable", wordstart);

					}else{
						char *v = vars_get(b, wordstart);
						if(v)
							pfunc("%s: %s", wordstart, *(char *)v ? "true" : "false");
						else
							pfunc("%s: (not set)", wordstart);
					}
					break;

				default:
					wrongfunc();
			}
			return;
		}
	}else if(*s == '\0' && !isset){
		/* set dump */
		enum varlist v = 0;

		do{
			const char *vs = vars_tostring(v);
			pfunc("%s: %d\n", vs, *(char *)vars_get(b, vs));
		}while(++v != VARS_SENTINEL);

		return;
	}
	wrongfunc();
}
