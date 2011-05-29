#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "range.h"
#include "buffer.h"
#include "command.h"

#include "buffer.h"
#include "util/list.h"
#include "vars.h"
#include "util/alloc.h"
#include "util/pipe.h"
#include "global.h"
#include "gui/motion.h"
#include "gui/gui.h"

#define LEN(x) ((signed)(sizeof(x) / sizeof(x[0])))

char *argv_to_str(int argc, char **argv)
{
	char *s;
	int len;
	int i;

	for(i = 0, len = 1; i < argc; i++)
		len += 1 + strlen(argv[i]);
	s = umalloc(len);
	*s = '\0';
	for(i = 0; i < argc; i++){
		strcat(s, argv[i]);
		strcat(s, " ");
	}
	return s;
}


void cmd_r(int argc, char **argv, int force, struct range *rng)
{
	char *cmd;

	if(argc != 2){
		gui_status("usage: r[!] cmd/file");
		return;
	}

	cmd = argv_to_str(argc, argv);

	if(force){
		struct list *l = pipe_read(cmd);
		if(!l)
			gui_status("pipe error: %s", strerror(errno));
		else if(l->data){
			buffer_insertlistafter(
					global_buffer,
					buffer_getindex(global_buffer, gui_y()),
					l);

			buffer_modified(global_buffer) = 1;
		}else{
			list_free(l);
			gui_status("%s: no output", cmd);
		}
	}else{
		buffer_t *tmpbuf = readfile(cmd, 0);

		if(!tmpbuf)
			gui_status("read: %s", strerror(errno));
		else{
			buffer_insertlistafter(global_buffer,
					buffer_getindex(global_buffer, gui_y()),
					buffer_gethead(tmpbuf));

			buffer_free_nolist(tmpbuf);

			buffer_modified(global_buffer) = 1;
		}
	}

	free(cmd);
}

void cmd_w(int argc, char **argv, int force, struct range *rng)
{
	enum { QUIT, EDIT, NONE } after = NONE;
	int nw;

	if(rng->start != -1 || rng->end != -1){
usage:
		gui_status("usage: w[qe][![!]] file|command");
		return;
	}else if(buffer_readonly(global_buffer)){
		gui_status("buffer is read-only");
		return;
	}

	if(!strcmp(argv[0], "wq"))
		after = QUIT;
	else if(!strcmp(argv[0], "we"))
		after = EDIT;
	else if(strcmp(argv[0], "w"))
		goto usage;

	if(argc > 1 && argv[1][0] == '!'){
		/* pipe */
		char *cmd = argv_to_str(argc, argv);
		char *bang = strchr(cmd, '!') + 1;

		shellout(bang, buffer_gethead(global_buffer));

		free(cmd);
		return;

	}else if(argc == 2 && after != EDIT){
		/* have a filename to save to */

		if(!force){
			struct stat st;

			if(!stat(argv[1], &st)){
				gui_status("not over-writing %s", argv[1]);
				return;
			}
		}
		buffer_setfilename(global_buffer, argv[1]);

	}else if(argc != 1){
		goto usage;

	}else if(!buffer_hasfilename(global_buffer)){
		gui_status("buffer has no filename");
		return;
	}

	nw = buffer_write(global_buffer);
	if(nw == -1){
		gui_status("Couldn't write \"%s\": %s", buffer_filename(global_buffer), strerror(errno));
		return;
	}
	buffer_modified(global_buffer) = 0;
	gui_status("\"%s\" %dL, %dC written", buffer_filename(global_buffer),
			buffer_nlines(global_buffer) - !buffer_eol(global_buffer), nw);

	switch(after){
		case EDIT:
			buffer_free(global_buffer);
			global_buffer = readfile(argv[1], 0);
			break;
		case QUIT:
			global_running = 0;
		case NONE:
			break;
	}
}


void cmd_q(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1 || rng->start != -1 || rng->end != -1){
		gui_status("usage: q[!]");
		return;
	}

	if(!force && buffer_modified(global_buffer))
		gui_status("unsaved");
	else
		global_running = 0;
}

void cmd_bang(int argc, char **argv, int force, struct range *rng)
{
	if(rng->start != -1 || rng->end != -1){
		/* rw!cmd command */
		struct list *l;

		if(argc < 2){
			gui_status("usage: [range]!cmd");
			return;
		}

		/* FIXME: more than one arg.. herpaderp */
		l = pipe_readwrite(argv[1], buffer_gethead(global_buffer));

		if(l){
			if(l->data){
				buffer_replace(global_buffer, l);
				buffer_modified(global_buffer) = 1;
			}else{
				list_free(l);
				gui_status("%s: no output", argv[1]);
			}
		}else{
			gui_status("pipe_readwrite() error: %s", strerror(errno));
		}
		return;
	}

	if(argc == 1){
		shellout("sh", NULL);
	}else{
		char *cmd = argv_to_str(argc, argv);
		shellout(cmd, NULL);
		free(cmd);
	}
}

void cmd_e(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 2){
		gui_status("usage: e[!] fname");
		return;
	}

	if(!force && buffer_modified(global_buffer)){
		gui_status("unsaved");
	}else{
		buffer_free(global_buffer);
		global_buffer = readfile(argv[1], 0);
		gui_move(0, 0);
	}
}

void cmd_new(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1)
		gui_status("usage: new[!]");

	buffer_free(global_buffer);
	global_buffer = readfile(argv[1], 0);
}

void cmd_set(int argc, char **argv, int force, struct range *rng)
{
	enum vartype type = 0;
	int i;

	if(argc == 1){
		/* set dump */

		do
			if(vars_isbool(type))
				gui_status_add("%s: %s", vars_tostring(type), *vars_get(type, global_buffer) ? "true" : "false");
			else
				gui_status_add("%s: %d", vars_tostring(type), *vars_get(type, global_buffer));
		while(++type != VARS_UNKNOWN);

		gui_status("any key to continue...");
		gui_peekch();
		gui_status("");
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

buffer_t *readfile(const char *filename, int ro)
{
	buffer_t *b = NULL;

	if(filename){
		int nread = buffer_read(&b, filename);

		if(nread == -1){
			b = buffer_new_empty();
			buffer_setfilename(b, filename);

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
				buffer_readonly(b) = 1;

			if(nread == 0)
				gui_status("(empty file)%s", buffer_readonly(b) ? " [read only]" : "");
			else
				gui_status("%s%s: %dC, %dL%s", filename,
						buffer_readonly(b) ? " [read only]" : "",
						buffer_nchars(b), buffer_nlines(b),
						buffer_eol(b) ? "" : " [noeol]");
		}
	}else{
newfile:
		/* new file */
		b = buffer_new_empty();
		gui_status("(new file)");
	}
	return b;
}

void shellout(const char *cmd, struct list *l)
{
	int ret;

	gui_term();

	if(l){
		if(pipe_write(cmd, l) == -1){
			int e = errno;
			gui_init();
			gui_status("pipe error: %s", strerror(e));
			return;
		}
		ret = 0;
	}else
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

void parsecmd(char *s, int *pargc, char ***pargv, int *pforce)
{
	char *cpy = ustrdup(s);
	char **argv;
	char *iter;
	int n = 1;

	for(iter = strtok(cpy, " !"); iter; iter = strtok(NULL, " "))
		n++;
	free(cpy);

	if(*s == '!')
		n++;

	argv = umalloc(sizeof(char *) * n);

	n = 0;
	if(*s == '!'){
		n++;
		argv[0] = "!";
	}

	if(s[1] == '!'){
		*pforce = 1;
		s[1] = ' ';
	}

	for(iter = strtok(s, " !"); iter; iter = strtok(NULL, " "))
		argv[n++] = iter;

	*pargv = argv;
	*pargc = n;
}

void command_run(char *in)
{
	static const struct
	{
		const char *nam;
		void (*f)(int argc, char **argv, int force, struct range *rng);
	} funcs[] = {
#define CMD(x) { #x, cmd_##x }
		{ "!",  cmd_bang },
		{ "we", cmd_w },
		{ "wq", cmd_w },
		CMD(r),
		CMD(w),
		CMD(q),
		CMD(e),
		CMD(set),
		CMD(new),
#undef CMD
	};

#define HAVE_RANGE (s > in)
	struct range lim, rng;
	char *s;
	char **argv;
	int argc;
	int force = 0;
	int found = 0;
	int i;

	if(!*in)
		return;

	lim.start = gui_y();
	lim.end		= buffer_nlines(global_buffer);

	s = parserange(in, &rng, &lim);

	if(!s){
		gui_status("couldn't parse range");
		return;
	}else if(HAVE_RANGE && *s == '\0'){
		/* just a number, move to that line */
		gui_move(rng.start - 1, gui_x());
		return;
	}

	if(!HAVE_RANGE)
		rng.start = rng.end = -1;

	parsecmd(s, &argc, &argv, &force);

	found = 0;
	for(i = 0; i < LEN(funcs); i++)
		if(!strcmp(argv[0], funcs[i].nam)){
			funcs[i].f(argc, argv, force, &rng);
			found = 1;
			break;
		}

	free(argv);

	if(!found)
		gui_status("not an editor command: \"%s\"", s);
}

void dumpbuffer(buffer_t *b)
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
		buffer_dump(b, f);
		fclose(f);
	}
}
