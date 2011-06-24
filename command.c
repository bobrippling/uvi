#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

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
#include "gui/intellisense.h"
#include "gui/gui.h"
#include "util/io.h"
#include "yank.h"

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
	if(force){
		struct list *l;
		char *cmd;

		cmd = argv_to_str(argc - 1, argv + 1);
		l = pipe_read(cmd);

		if(!l){
			gui_status(GUI_ERR, "pipe error: %s", strerror(errno));
		}else if(l->data){
			buffer_insertlistafter(
					global_buffer,
					buffer_getindex(global_buffer, gui_y()),
					l);

			buffer_modified(global_buffer) = 1;
		}else{
			list_free(l, free);
			gui_status(GUI_ERR, "%s: no output", cmd);
		}

		free(cmd);

	}else if(argc == 2){
		struct list *l = list_from_filename(argv[1], NULL);

		if(l){
			buffer_insertlistafter(
					global_buffer,
					buffer_getindex(global_buffer, gui_y()),
					l);

			buffer_modified(global_buffer) = 1;
		}else{
			gui_status(GUI_ERR, "read: %s", strerror(errno));
		}

	}else{
		gui_status(GUI_ERR, "usage: r[!] cmd/file");
		return;
	}

}

void cmd_w(int argc, char **argv, int force, struct range *rng)
{
	enum { QUIT, EDIT, NONE } after = NONE;
	int nw;
	int x = 0;

	if(rng->start != -1 || rng->end != -1){
usage:
		gui_status(GUI_ERR, "usage: w[qe][![!]] file|command");
		return;
	}else if(buffer_readonly(global_buffer)){
		gui_status(GUI_ERR, "buffer is read-only");
		return;
	}

	if(!strcmp(argv[0], "wq")){
		after = QUIT;
	}else if(!strcmp(argv[0], "x")){
		after = QUIT;
		x = 1;

	}else if(!strcmp(argv[0], "we")){
		after = EDIT;
	}else if(strcmp(argv[0], "w")){
		goto usage;
	}

	if(argc > 1 && argv[1][0] == '!'){
		/* pipe */
		char *cmd = argv_to_str(argc - 1, argv + 1);
		char *bang = strchr(cmd, '!') + 1;

		shellout(bang, buffer_gethead(global_buffer));

		free(cmd);
		return;

	}else if(argc == 2 && after != EDIT){
		/* have a filename to save to */

		if(!force){
			struct stat st;

			if(!stat(argv[1], &st)){
				gui_status(GUI_ERR, "not over-writing %s", argv[1]);
				return;
			}
		}
		buffer_setfilename(global_buffer, argv[1]);

	}else if(argc != 1 && (after == EDIT ? argc != 2 : 0)){
		goto usage;

	}

	if(x && !buffer_modified(global_buffer))
		goto after;

	if(!buffer_hasfilename(global_buffer)){
		gui_status(GUI_ERR, "buffer has no filename");
		return;
	}

	if(!force && buffer_external_modified(global_buffer)){
		gui_status(GUI_ERR, "buffer changed externally since last read");
		return;
	}

	nw = buffer_write(global_buffer);
	if(nw == -1){
		gui_status(GUI_ERR, "couldn't write \"%s\": %s", buffer_filename(global_buffer), strerror(errno));
		return;
	}
	buffer_modified(global_buffer) = 0;
	gui_status(GUI_NONE, "\"%s\" %dL, %dC written", buffer_filename(global_buffer),
			buffer_nlines(global_buffer) - !buffer_eol(global_buffer), nw);

after:
	switch(after){
		case EDIT:
			buffer_free(global_buffer);
			global_buffer = readfile(argv[1]);
			gui_move(0, 0);
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
		gui_status(GUI_ERR, "usage: q[!]");
		return;
	}

	if(!force && buffer_modified(global_buffer))
		gui_status(GUI_ERR, "unsaved");
	else
		global_running = 0;
}

void cmd_bang(int argc, char **argv, int force, struct range *rng)
{
	if(rng->start != -1 || rng->end != -1){
		/* rw!cmd command */
		struct list *l;

		if(argc < 2){
			gui_status(GUI_ERR, "usage: [range]!cmd");
			return;
		}

		/* FIXME: more than one arg.. herpaderp */
		l = pipe_readwrite(argv[1], buffer_gethead(global_buffer));

		if(l){
			if(l->data){
				buffer_replace(global_buffer, l);
				buffer_modified(global_buffer) = 1;
			}else{
				list_free(l, free);
				gui_status(GUI_ERR, "%s: no output", argv[1]);
			}
		}else{
			gui_status(GUI_ERR, "pipe_readwrite() error: %s", strerror(errno));
		}
		return;
	}

	if(argc == 1){
		shellout("sh", NULL);
	}else{
		char *cmd = argv_to_str(argc - 1, argv + 1);
		shellout(cmd, NULL);
		free(cmd);
	}
}

void cmd_e(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 2){
		gui_status(GUI_ERR, "usage: e[!] fname");
		return;
	}

	if(!force && buffer_modified(global_buffer)){
		gui_status(GUI_ERR, "unsaved");
	}else{
		buffer_free(global_buffer);
		global_buffer = readfile(argv[1]);
		gui_move(0, 0);
	}
}

void cmd_new(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1){
		gui_status(GUI_ERR, "usage: new[!]");
		return;
	}

	if(!force && buffer_modified(global_buffer)){
		gui_status(GUI_ERR, "new: buffer modified");
	}else{
		buffer_free(global_buffer);
		global_buffer = buffer_new_empty();
		gui_move(0, 0);
	}
}

void cmd_where(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1 || force || rng->start != -1 || rng->end != -1)
		gui_status(GUI_ERR, "usage: %s", *argv);
	else
		gui_status(GUI_NONE, "x=%d y=%d left=%d top=%d",
				gui_x(), gui_y(), gui_left(), gui_top());
}

void cmd_set(int argc, char **argv, int force, struct range *rng)
{
	enum vartype type = 0;
	int i;
	int wait = 0;

	if(argc == 1){
		/* set dump */
		do
			vars_show(type++);
		while(type != VARS_UNKNOWN);
		wait = 1;
	}else
		for(i = 1; i < argc; i++){
			char *wordstart;
			int bool, gotbool = 0;
			enum { NONE, QUERY, FLIP } mode = NONE;
			int j;

			if(!strncmp(wordstart = argv[i], "no", 2)){
				bool = 0;
				wordstart += 2;
				gotbool = 1;
			}else
				bool = 1;

			j = strlen(wordstart)-1;
			if(wordstart[j] == '?' || wordstart[j] == '!'){
				mode = wordstart[j] == '?' ? QUERY : FLIP;
				wordstart[j] = '\0';
			}

			if((type = vars_gettype(wordstart)) == VARS_UNKNOWN){
				gui_status(GUI_ERR, "unknown variable \"%s\"", wordstart);
				return;
			}

			if(mode != NONE){
				switch(mode){
					case QUERY:
						vars_show(type);
						wait = 1;
						break;
					case FLIP:
						vars_set(type, global_buffer, !vars_get(type, global_buffer));
						break;

					default:
						break;
				}

			}else if(vars_isbool(type)){
				vars_set(type, global_buffer, bool);
			}else if(gotbool){
				gui_status(GUI_ERR, "\"%s\" is not a bool", vars_tostring(type));
				return;
			}else{
				if(++i == argc){
					gui_status(GUI_ERR, "need value for \"%s\"", vars_tostring(type));
					return;
				}else{
					vars_set(type, global_buffer, atoi(argv[i]));
				}
			}
		}

	if(wait)
		gui_status_wait();
}

void cmd_regs(int argc, char **argv, int force, struct range *rng)
{
	int one = 0;
	int i;

	if(argc != 1 || force || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s", *argv);
		return;
	}

	for(i = 'a' - 1; i <= 'z'; i++){
		struct yank *y = yank_get(i);
		char c = i == 'a'-1 ? '"': i;

		if(y->v){
			if(y->is_list)
				gui_status_add(GUI_NONE, "%c: list, head: \"%s\"", c, (const char *)(((struct list *)y->v)->data));
			else
				gui_status_add(GUI_NONE, "%c: string:     \"%s\"", c, (const char *)y->v);

			one = 1;
		}
	}

	if(one)
		gui_status_wait();
	else
		gui_status(GUI_ERR, "no yanks");
}

#ifdef BLOAT
# include "bloat/command.c"
#endif

int shellout(const char *cmd, struct list *l)
{
	int ret;

	gui_term();

	if(l){
		if(pipe_write(cmd, l, 0) == -1){
			int e = errno;
			gui_reload();
			gui_status(GUI_ERR, "pipe error: %s", strerror(e));
			return -1;
		}
		ret = 0;
	}else
		ret = system(cmd);

	if(ret == -1)
		perror("system()");
	else if(WEXITSTATUS(ret))
		printf("%s returned %d\n", cmd, WEXITSTATUS(ret));

	fputs("Any key to continue...\n", stdout);
	fflush(stdout);

	chomp_line();

	gui_reload();

	return ret == -1 ? -1 : WEXITSTATUS(ret);
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

void char_replace(int c, const char *rep, int argc, char **argv)
{
	int i;

	for(i = 0; i < argc; i++){
		char *p = strchr(argv[i], c);

		if(p && (p > argv[i] ? p[-1] != '\\' : 1)){
			char *sav = argv[i];
			*p++ = '\0';

			argv[i] = ustrprintf("%s%s%s", argv[i], rep, p);
			free(sav);
		}
	}
}

void filter_cmd(int argc, char **argv)
{
	const char *home;
	int i;

	if(!(home = getenv("HOME")))
		home = "/";

	for(i = 0; i < argc; i++)
		argv[i] = ustrdup(argv[i]);

	char_replace('~', home, argc, argv);

	if(buffer_hasfilename(global_buffer))
		char_replace('#', buffer_filename(global_buffer), argc, argv);
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
		{ "?",  cmd_where },
		{ "we", cmd_w },
		{ "wq", cmd_w },
		{ "x",  cmd_w },
		CMD(new),
		CMD(r),
		CMD(w),
		CMD(q),
		CMD(e),
		CMD(set),
		CMD(new),
		CMD(regs),
#ifdef BLOAT
# include "bloat/command.h"
#endif

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
		gui_status(GUI_ERR, "couldn't parse range");
		return;
	}else if(HAVE_RANGE && *s == '\0'){
		/* just a number, move to that line */
		gui_move(rng.start - 1, gui_x());
		return;
	}

	if(!HAVE_RANGE)
		rng.start = rng.end = -1;

	parsecmd(s, &argc, &argv, &force);
	filter_cmd(argc, argv);

	found = 0;
	for(i = 0; i < LEN(funcs); i++)
		if(!strcmp(argv[0], funcs[i].nam)){
			funcs[i].f(argc, argv, force, &rng);
			found = 1;
			break;
		}

	for(i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);

	if(!found)
		gui_status(GUI_ERR, "not an editor command: \"%s\"", s);
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
