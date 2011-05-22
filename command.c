#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "buffer.h"
#include "util/list.h"
#include "vars.h"
#include "util/alloc.h"
#include "util/pipe.h"
#include "global.h"

#define LEN(x) (sizeof(x) / sizeof(x[0]))

void cmd_r(int argc, char **argv, struct range *rng)
{
#if 0
	if(s[1] == '!'){
		char *cmd = s + 2;
		while(isspace(*cmd))
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
			char *cmd = s + 3;
			struct list *l;

			while(isspace(*cmd))
				cmd++;

			l = pipe_readwrite(cmd, buffer_gethead(buffer));

			if(l){
				if(l->data){
					buffer_replace(buffer, l);
					buffer_modified(buffer) = 1;
				}else{
					list_free(l);
					pfunc("%s: no output", cmd);
				}
			}else{
				pfunc("pipe_readwrite() error: %s", strerror(errno));
			}
		}
	}else{
		char *cmd = s + 1;
		while(isspace(*cmd))
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
#endif
}

void cmd_w(int argc, char **argv, struct range *rng)
{
	/* brace for spaghetti */
	char bail = 0, edit = 0, *fname;

#if 0
	if(s[1] == '!'){
		/* write [TODO: range] to pipe */
		char *cmd = s + 2;
		while(isspace(*cmd))
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
#endif
	gui_status("TODO :w");
}


void cmd_q(int argc, char **argv, struct range *rng)
{
	if(argc != 1 || rng->start != -1 || rng->end != -1){
usage:
		gui_status("usage: q[!]");
		return;
	}

	if(!strcmp(argv[0], "q!"))
		global_running = 0;
	else if(!strcmp(argv[0], "q"))
		if(buffer_modified(global_buffer))
			gui_status("unsaved");
		else
			global_running = 0;
	else
		goto usage;
}

void cmd_bang(int argc, char **argv, struct range *rng)
{
	int i;

	if(rng->start != -1 || rng->end != -1){
		gui_status("TODO: :<range>!...");
		return;
	}

	for(i = 0; i < argc - 1; i++)
		argv[i][strlen(argv[i])] = ' ';

	shellout(argv[0] + 1);
}

void cmd_e(int argc, char **argv, struct range *rng)
{
	int force = 0;

	if(argc != 2){
usage:
		gui_status("usage: e[!] fname");
		return;
	}

	if(!strcmp(argv[0], "e!"))
		force = 1;
	else if(strcmp(argv[0], "e"))
		goto usage;

	if(!force && buffer_modified(global_buffer)){
		gui_status("unsaved");
	}else{
		buffer_free(global_buffer);
		readfile(argv[1], 0);
		gui_clip();
	}
}

void cmd_set(int argc, char **argv, struct range *rng)
{
	enum vartype type = 0;
	int i;

	if(strcmp(argv[0], "set")){
		gui_status("usage: set var [value]");
		return;
	}


	if(argc == 1){
		/* set dump */
		do
			gui_status_add("%s: %d", vars_tostring(type), *vars_get(type, global_buffer));
		while(++type != VARS_UNKNOWN);
		gui_status("any key to continue...");
		gui_anykey();
		return;
	}

	for(i = 1; i < argc; i++){
		char *wordstart;
		int bool, gotbool = 0;

		if(!strncmp(wordstart = argv[1], "no", 2)){
			bool = 0;
			wordstart += 2;
			gotbool = 1;
		}else
			bool = 1;

		if((type = vars_gettype(wordstart)) == VARS_UNKNOWN){
			gui_status("unknown variable \"%s\"", wordstart);
			return;
		}

		if(vars_isbool(type)){
			vars_set(type, global_buffer, bool);
		}else if(gotbool){
			gui_status("\"%s\" is not a bool", vars_tostring(type));
			return;
		}else{
			if(++i == argc)
				gui_status("need value for \"%s\"", vars_tostring(type));
			else
				vars_set(type, global_buffer, atoi(argv[i]));
		}
	}
}

void readfile(const char *filename, int ro)
{
	if(filename){
		int nread = buffer_read(&global_buffer, filename);

		if(nread == -1){
			global_buffer = buffer_new_empty();
			buffer_setfilename(global_buffer, filename);

			if(errno != ENOENT)
				/*
				 * end up here on failed read:
				 * open empty file and continue
				 */
				gui_status("\"%s\" [%s]", filename, errno ? strerror(errno) : "unknown error - binary file?");
			else
				/* something like "./uvi file_that_doesn\'t_exist */
				goto newfile;

		}else{
			/* end up here on successful read */
			if(ro)
				buffer_readonly(global_buffer) = 1;

			if(nread == 0)
				gui_status("(empty file)%s", buffer_readonly(global_buffer) ? " [read only]" : "");
			else
				gui_status("%s%s: %dC, %dL%s", filename,
						buffer_readonly(global_buffer) ? " [read only]" : "",
						buffer_nchars(global_buffer), buffer_nlines(global_buffer),
						buffer_eol(global_buffer) ? "" : " [noeol]");
		}
	}else{
newfile:
		/* new file */
		global_buffer = buffer_new_empty();
		gui_status("(new file)");
	}
}

void shellout(const char *cmd)
{
	int ret;

	gui_term();
	ret = system(cmd);

	if(ret == -1)
		perror("system()");
	else if(WEXITSTATUS(ret))
		printf("%s returned %d\n", cmd, WEXITSTATUS(ret));

	fputs("Press enter to continue...", stdout);
	fflush(stdout);
	ret = getchar();

	if(ret != '\n' && ret != EOF)
		while((ret = getchar()) != '\n' && ret != EOF);

	gui_init();
}

void command_run(char *in)
{
	static const struct
	{
		const char *nam;
		void (*f)(int argc, char **argv, struct range *rng);
	} funcs[] = {
#define CMD(x) { #x, cmd_##x }
		{ "!", cmd_bang },
		CMD(r),
		CMD(w),
		CMD(q),
		CMD(e),
		CMD(set),
#undef CMD
	};

#define HAVE_RANGE (s > in)
	struct range lim, rng;
	char *s, *iter;
	char **argv;
	int argc;
	int found = 0;
	int i;

	lim.start = global_y;
	lim.end		= buffer_nlines(global_buffer);

	s = parserange(in, &rng, &lim);

	if(!s){
		gui_status("couldn't parse range");
		return;
	}else if(HAVE_RANGE && *s == '\0'){
		/* just a number, move to that line */
		global_y = rng.start - 1; /* -1, because they enter between 1 & $ */
		return;
	}

	if(!HAVE_RANGE)
		rng.start = rng.end = -1;

	argc = 1;
	for(iter = s; *iter; iter++)
		if(*iter == ' ' && (iter > s ? iter[-1] != '\\' : 1))
			argc++;

	argv = umalloc(sizeof(*argv) * argc);
	argv[0] = s;

	i = 1;
	for(iter = s; *iter; iter++)
		if(*iter == ' ' && (iter > s ? iter[-1] != '\\' : 1)){
			argv[i++] = iter + 1;
			*iter = '\0';
		}

	for(i = 0; i < LEN(funcs); i++)
		if(!strncmp(s, funcs[i].nam, strlen(funcs[i].nam))){
			funcs[i].f(argc, argv, &rng);
			found = 1;
			break;
		}

	free(argv);

	if(!found && *s)
		gui_status("not an editor command: \"%s\"", s);
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
