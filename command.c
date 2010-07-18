#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#include "config.h"

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "buffer.h"
#include "util/list.h"
#include "vars.h"
#include "util/alloc.h"
#include "util/pipe.h"

extern buffer_t *lastbuffer;

static char *prevcmd = NULL;

static void parse_setget(buffer_t *, char, char *, void (*)(const char *, ...), void (*)(void));
static buffer_t *newemptybuffer(void);
static void command_e(char *s, buffer_t **b, int *curline, void (*const pfunc)(const char *, ...));

void command_free()
{
	free(prevcmd);
	prevcmd = NULL;
}

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
							longjmp(allocerr, ALLOC_COMMAND_C);
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
	buffer_t **b,
	/* Note: curline is the index, i.e. 0 to $-1 */
	void (*wrongfunc)(void),
	void (*pfunc)(const char *, ...),
	enum gret (*gfunc)(char *, int),
	int  (*qfunc)(const char *, ...),
	void (*shellout)(const char *),
	void (*ui_toggle)(char up)
	)
{
#define buffer (*b)
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

		case 'r':
		{
			if(s[1] == '!'){
				char *cmd = s + 2;
				while(*cmd == ' ')
					cmd++;

				if(!strlen(cmd))
					pfunc("need command to read from");
				else{
					struct list *l = pipe_read(cmd);
					if(!l)
						pfunc("pipe error: %s", strerror(errno));
					else if(l->data){
						buffer_insertlistafter(buffer,
								buffer_getindex(buffer, *curline),
								l);

						buffer_modified(buffer) = 1;
					}else{
						list_free(l);
						pfunc("%s: no output", cmd);
					}
				}
			}else if(s[1] == 'w'){
				/* rw!cmd command */
				if(s[2] != '!')
					wrongfunc();
				else{
					/* TODO */
					pfunc("!rw: TODO");
				}
			}else{
				char *cmd = s + 1;
				while(*cmd == ' ')
					cmd++;

				if(!strlen(cmd))
					pfunc("need file to read from");
				else{
					buffer_t *tmpbuf = command_readfile(cmd, 0, pfunc);
					if(!tmpbuf)
						pfunc("read: %s", strerror(errno));
					else{
						buffer_insertlistafter(buffer,
								buffer_getindex(buffer, *curline),
								buffer_gethead(tmpbuf));

						buffer_free_nolist(tmpbuf);

						buffer_modified(buffer) = 1;
					}
				}
			}
			break;
		}

		case 'w':
		{
			/* brace for spaghetti */
			char bail = 0, edit = 0, *fname;

			if(s[1] == '!'){
				/* write [TODO: range] to pipe */
				char *cmd = s + 2;
				while(*cmd == ' ')
					cmd++;

				if(!strlen(cmd))
					pfunc("need command to pipe to");
				else{
					if(ui_toggle)
						ui_toggle(0);

					if(pipe_write(cmd, buffer_gethead(buffer)) == -1)
						pfunc("pipe error: %s", strerror(errno));

					if(ui_toggle){
						int c;
						fputs("Press enter to continue", stdout);
						fflush(stdout);

						c = getchar();
						if(c != '\n' && c != EOF)
							while((c = getchar()) != '\n' && c != EOF);
					}

						ui_toggle(1);
				}
				break;
			}

			if(HAVE_RANGE){
				wrongfunc();
				break;
			}else if(buffer_readonly(buffer)){
				pfunc(READ_ONLY_ERR);
				break;
			}

			switch(strlen(s)){
				case 2:
					if(s[1] == 'q')
						flag = 1;
					else{
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
							if(s[2] != ' '){
								bail = 1;
								wrongfunc();
								break;
							}
vars_fname:
							if(strcmp(buffer_filename(buffer), fname)){
								struct stat st;

								if(stat(fname, &st)){
									if(errno != ENOENT && !qfunc("couldn't stat %s: %s, write? ", fname, strerror(errno)))
										return 1;
								}else if(!qfunc("%s exists, write? ", fname))
									return 1;
							}

							buffer_setfilename(buffer, fname);
							break;

						case 'e':
							/*
							 * :we fname
							 * what we do is set a flag,
							 * write the file, then
							 * do the :e bit (pass it
							 * s + 1)
							 */
							edit = 1;
							/* fname is for :e, later */
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
			if(edit){
				command_e(s + 1, b, curline, pfunc);
				break;
			}else if(!flag)
				/* i.e. 'q' hasn't been passed */
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
				goto def; /* could be ':get' */
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
				if(!strcmp(s+1, "!"))
					if(prevcmd)
						shellout(prevcmd);
					else
						pfunc("no previous command");
				else if(s[1] == '\0')
					shellout("sh");
				else{
					free(prevcmd);
					prevcmd = umalloc(strlen(s));
					strcpy(prevcmd, s+1);
					shellout(s + 1);
				}
			}
			break;

		case 'e':
			if(HAVE_RANGE){
				wrongfunc();
			}else
				command_e(s, b, curline, pfunc);
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
#undef HAVE_RANGE
}

static void command_e(char *s, buffer_t **b, int *y,
		void (*const pfunc)(const char *, ...))
{
	char force = 0, *fname = s + 1;

	if(s[1] == '!'){
		fname = s + 2;
		force = 1;
	}

	while(*fname == ' ')
		fname++;

	if(!force && buffer_modified(buffer))
		pfunc("unsaved");
	else{
		buffer_free(buffer);
		if(strlen(fname))
			buffer = command_readfile(fname, 0, pfunc);
		else{
			buffer = newemptybuffer();
			pfunc("new empty buffer");
		}

		/*if(*y >= (nlines = buffer_nlines(buffer)))
			*y = nlines - 1;*/
		*y = 0;
	}
#undef buffer
}

buffer_t *command_readfile(const char *filename, char forcereadonly, void (*const pfunc)(const char *, ...))
{
  buffer_t *buffer;
	if(filename){
		int nread = buffer_read(&buffer, filename);

		if(nread < 0){
			buffer = newemptybuffer();
			buffer_setfilename(buffer, filename);

			if(errno != ENOENT){
				/*
				 * end up here on failed read:
				 * open empty file and continue
				 */
				pfunc("\"%s\" [%s]", filename, errno ? strerror(errno) : "unknown error - binary file?");
				buffer_readonly(buffer) = 1;
			}else
				/* something like "./uvi file_that_doesn\'t_exist */
				goto newfile;

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
						buffer_eol(buffer) ? "" : " [noeol]");
		}

	}else{
		/* new file */
		buffer = newemptybuffer();
newfile:
		pfunc("(new file)");
	}

	lastbuffer = buffer;
  return buffer;
}

static buffer_t *newemptybuffer()
{
	char *s = umalloc(sizeof(char));
	*s = '\0';

	return buffer_new(s);
}

void command_dumpbuffer(buffer_t *b)
{
	char dump_postfix[] = "_dump_a";
	FILE *f;
	struct stat st;
	char *path, freepath = 0, *prefixletter;

	if(!b)
		return;

	path = dump_postfix;

	if(buffer_hasfilename(b)){
		char *const s = malloc(strlen(buffer_filename(b)) + strlen(dump_postfix) + 1);
		if(s){
			freepath = 1;
			strcpy(s, buffer_filename(b));
			strcat(s, dump_postfix);
			path = s;
		}
	}

	prefixletter = path + strlen(path) - 1;
	/*
	 * if file exists, increase dump prefix,
	 * else (and on stat error), save
	 */
	do
		if(stat(path, &st) == 0)
			/* exists */
			++*prefixletter;
		else
			break;
	while(*prefixletter < 'z');
	/* if it's 'z', the user should clean their freakin' directory */

	f = fopen(path, "w");

	if(freepath)
		free(path);

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
			char *wordstart = s, bool, tmp;
			enum vartype type;

			if(!strncmp(wordstart, "no", 2) && isset){
				bool = 0;
				wordstart += 2;
			}else
				bool = 1;

			while(isalpha(*++s));

			tmp = *s;
			*s = '\0';

			if((type = vars_gettype(wordstart)) == VARS_UNKNOWN){
				pfunc("unknown variable");
				return;
			}

			switch(tmp){
				case '\0':
					if(isset){
						if(vars_isbool(type))
							vars_set(type, b, bool);
						else
							pfunc("\"%s\" needs an integer value", wordstart);
					}else
						pfunc("%s: %d", wordstart, *vars_get(type, b));
					break;

				case ' ':
					if(isset){
						char val = atoi(++s);

						if(!val)
							pfunc("\"%s\" must be a number > 0", s);
						else
							vars_set(type, b, val);
					}else{
						char *p = vars_get(type, b);
						if(p)
							pfunc("%s: %d");
						else
							pfunc("%s: (not set)");
					}
					break;

				default:
					wrongfunc();
			}
			return;
		}
	}else if(*s == '\0' && !isset){
		/* set dump */
		enum vartype t = 0;

		do
			pfunc("%s: %d\n", vars_tostring(t), *vars_get(t, b));
		while(++t != VARS_UNKNOWN);

		return;
	}
	wrongfunc();
}
