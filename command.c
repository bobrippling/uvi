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
#include <unistd.h>

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
#include "buffers.h"
#include "util/str.h"
#include "rc.h"

#define LEN(x) ((signed)(sizeof(x) / sizeof(x[0])))

#define MODIFIED_CHECK() \
	do if(!force && buffer_modified(buffers_current())){ \
		gui_status(GUI_ERR, "buffer modified since last write"); \
		return; \
	} while(0)


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
					buffers_current(),
					buffer_getindex(buffers_current(), gui_y()),
					l);

			buffer_modified(buffers_current()) = 1;
		}else{
			list_free(l, free);
			gui_status(GUI_ERR, "%s: no output", cmd);
		}

		free(cmd);

	}else if(argc == 2){
		struct list *l = list_from_filename(argv[1], NULL);

		if(l){
			buffer_insertlistafter(
					buffers_current(),
					buffer_getindex(buffers_current(), gui_y()),
					l);

			buffer_modified(buffers_current()) = 1;
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
	struct list *list_to_write = NULL;
	enum { QUIT, EDIT, NONE } after = NONE;
	int nw, nl;
	int x = 0;

	if(rng->start != -1){
		if(rng->end == -1)
			rng->start = rng->end;

		if(--rng->start < 0)
			rng->start = 0;

		list_to_write = buffer_copy_range(buffers_current(), rng);

		if(argv[0][0] == '!'){
			char *cmd = argv_to_str(argc, argv);
			char *bang = strchr(cmd, '!') + 1;

			shellout(bang, list_to_write);

			free(cmd);
			list_free(list_to_write, free);
			return;
		}
	}

	if(!strcmp(argv[0], "wq")){
		after = QUIT;
	}else if(!strcmp(argv[0], "x")){
		after = QUIT;
		x = 1;
	}else if(!strcmp(argv[0], "we")){
		after = EDIT;
	}else if(strcmp(argv[0], "w")){
usage:
		gui_status(GUI_ERR, "usage: w[qe][![!]] file|command");
		goto fin;
	}

	if(argc > 1 && argv[1][0] == '!'){
		/* same as above pipe, except the whole file */
		char *cmd = argv_to_str(argc - 1, argv + 1);
		char *bang = strchr(cmd, '!') + 1;

		shellout(bang, buffer_gethead(buffers_current()));

		free(cmd);
		goto fin;
	}

	/* past the point of ! commands */
	if(argc > 2)
		goto usage;

	if(argc == 2 && after != EDIT){
		/* have a filename to save to */
		if(!force){
			struct stat st;

			if(stat(argv[1], &st) == 0){
				gui_status(GUI_ERR, "not over-writing %s", argv[1]);
				goto fin;
			}
		}
		buffer_setfilename(buffers_current(), argv[1]);
		buffer_modified(buffers_current()) = 1;
	}

	if(!buffer_hasfilename(buffers_current())){
		gui_status(GUI_ERR, "buffer has no filename");
		goto fin;
	}

	if(x && !buffer_modified(buffers_current()))
		goto after;

	if(!force){
		if(buffer_readonly(buffers_current())){
			gui_status(GUI_ERR, "buffer is read-only");
			goto fin;
		}
		if(buffer_external_modified(buffers_current())){
			gui_status(GUI_ERR, "buffer changed externally since last read");
			goto fin;
		}
	}

	if(list_to_write){
		nw = buffer_write_list(buffers_current(), list_to_write);
		nl = list_count(list_to_write);
		list_free(list_to_write, free);
	}else{
		nw = buffer_write(buffers_current());
		nl = buffer_nlines(buffers_current()) - !buffer_eol(buffers_current());
	}

	if(nw == -1){
		gui_status(GUI_ERR, "couldn't write \"%s\": %s", buffer_filename(buffers_current()), strerror(errno));
		goto fin;
	}

	buffer_modified(buffers_current()) = 0;
	gui_status(GUI_NONE, "\"%s\" %s%dL, %dC written",
			buffer_filename(buffers_current()),
			list_to_write ? "[partial-range] ":"",
			nl, nw);

after:
	switch(after){
		case EDIT:
			buffers_load(argv[1]);
			break;
		case QUIT:
			global_running = 0;
		case NONE:
			break;
	}

fin:
	if(list_to_write)
		list_free(list_to_write, free);
}


void cmd_q(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1 || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: q[!]");
		return;
	}

	MODIFIED_CHECK();

	global_running = 0;
}

void cmd_bang(int argc, char **argv, int force, struct range *rng)
{
	if(rng->start != -1){
		/* rw!cmd command */
		struct list *l, *to_pipe;
		char *free_me = argv_to_str(argc, argv);
		char *cmd = strchr(free_me, '!') + 1;

		if(argc <= 1){
			gui_status(GUI_ERR, "usage: range!cmd");
			return;
		}

		if(--rng->start < 0)
			rng->start = 0;
		if(rng->end == -1)
			rng->end = rng->start;

		to_pipe = buffer_extract_range(buffers_current(), rng);

		l = pipe_readwrite(cmd, to_pipe ? to_pipe : buffer_gethead(buffers_current()));

		if(l){
			if(l->data){
				if(to_pipe){
					struct list *inshere;

					inshere = buffer_getindex(buffers_current(), rng->start);

					if(!inshere)
						inshere = buffer_gettail(buffers_current());

					buffer_insertlistafter(buffers_current(), inshere, l);

					list_free_nodata(to_pipe);
					to_pipe = NULL;
				}else{
					buffer_replace(buffers_current(), l);
				}

				buffer_modified(buffers_current()) = 1;
			}else{
				list_free(l, free);
				gui_status(GUI_ERR, "%s: no output", cmd);
			}
		}else{
			gui_status(GUI_ERR, "pipe_readwrite() error: %s", strerror(errno));
		}

		buffer_modified(buffers_current()) = 1;

		free(free_me);
		if(to_pipe)
			list_free(to_pipe, free);
	}else{
		if(argc == 1){
			const char *shell = getenv("SHELL");
			if(!shell)
				shell = "sh";
			shellout(shell, NULL);
		}else{
			char *cmd = argv_to_str(argc - 1, argv + 1);
			shellout(cmd, NULL);
			free(cmd);
		}
	}
}

void cmd_e(int argc, char **argv, int force, struct range *rng)
{
	if(argc > 2 || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: e[!] fname");
		return;
	}

	MODIFIED_CHECK();

	if(argc == 1 && !buffer_hasfilename(buffers_current())){
		gui_status(GUI_ERR, "no filename");
		return;
	}

	buffers_load(argc == 1 ? buffer_filename(buffers_current()) : argv[1]);
}

void cmd_new(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1 || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: new[!]");
		return;
	}

	MODIFIED_CHECK();

	buffers_load(NULL);
}

void cmd_where(int argc, char **argv, int force, struct range *rng)
{
	char *line;
	if(argc != 1 || force || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s", *argv);
		return;
	}

	line = buffer_getindex(buffers_current(), gui_y())->data;

	gui_status(GUI_NONE, "x=%d y=%d left=%d top=%d char=%d",
			gui_x(), gui_y(), gui_left(), gui_top(), line[gui_x()]);
}

void cmd_set(int argc, char **argv, int force, struct range *rng)
{
	enum vartype type;
	int i;
	int wait = 0;

	gui_status_add_start();
	if(argc == 1){
		/* set dump */
		for(type = 0; type != VARS_UNKNOWN; type++)
			vars_show(type);
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
						vars_set(type, buffers_current(), !vars_get(type, buffers_current()));
						break;

					default:
						break;
				}

			}else if(vars_isbool(type)){
				vars_set(type, buffers_current(), bool);
			}else if(gotbool){
				gui_status(GUI_ERR, "\"%s\" is not a bool", vars_tostring(type));
				return;
			}else{
				if(++i == argc){
					gui_status(GUI_ERR, "need value for \"%s\"", vars_tostring(type));
					return;
				}else{
					vars_set(type, buffers_current(), atoi(argv[i]));
				}
			}
		}

	if(wait)
		gui_status_wait(-1, -1, NULL);
}

void cmd_yanks(int argc, char **argv, int force, struct range *rng)
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

		gui_status_add_start();
		if(y->v){
			if(y->is_list)
				gui_status_add(GUI_NONE, "%c: list, head: \"%s\"", c, (const char *)(((struct list *)y->v)->data));
			else
				gui_status_add(GUI_NONE, "%c: string:     \"%s\"", c, (const char *)y->v);

			one = 1;
		}
	}

	if(one)
		gui_status_wait(-1, -1, NULL);
	else
		gui_status(GUI_ERR, "no yanks");
}

void cmd_A(int argc, char **argv, int force, struct range *rng)
{
	char *bfname, *fname;
	char *ext;
	int len;


	if(argc != 1 || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s[!]", *argv);
		return;
	}

	MODIFIED_CHECK();

	if(!buffer_hasfilename(buffers_current())){
		gui_status(GUI_ERR, "buffer has no filename");
		return;
	}

	len = strlen(buffer_filename(buffers_current()));
	bfname = ALLOCA(len + 1);
	strcpy(bfname, buffer_filename(buffers_current()));

	ext = strrchr(bfname, '.');
	if(!ext){
		gui_status(GUI_ERR, "no file extension");
		return;
	}

	*ext++ = '\0';

	if(!strcmp(ext, "c") || !strcmp(ext, "cpp")){
		fname = ustrprintf("%s.h",  bfname);
	}else if(!strcmp(ext, "h")){
		char *try;

		fname = ustrprintf("%s.c",  bfname);

		if(access(fname, F_OK)){
			try = ustrprintf("%s.cpp",  bfname);
			if(!access(try, F_OK)){
				free(fname);
				fname = try;
			}else{
				free(try);
			}
		}
	}

	buffers_load(fname);
	free(fname);

	return;
}

void cmd_n(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 1 || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s[!]", *argv);
		return;
	}

	MODIFIED_CHECK();

	if(buffers_next(**argv == 'n' ? 1 : -1))
		gui_status(GUI_ERR, "file index out of range");
}

void cmd_ls(int argc, char **argv, int force, struct range *rng)
{
	const int cur = buffers_idx();
	struct old_buffer **iter;
	int i;

	if(argc != 1 || rng->start != -1 || rng->end != -1 || force){
		gui_status(GUI_ERR, "usage: %s", *argv);
		return;
	}

	gui_status_add_start();
	for(i = 0, iter = buffers_array(); *iter; iter++, i++)
		gui_status_add(i == cur ? GUI_COL_BLUE : GUI_NONE, "%d: %s", i, (*iter)->fname);
	gui_status_wait(-1, -1, NULL);
}

void cmd_b(int argc, char **argv, int force, struct range *rng)
{
	int n;

	if(argc != 2 || rng->start != -1 || rng->end != -1){
usage:
		gui_status(GUI_ERR, "usage: %s idx", *argv);
		return;
	}

	MODIFIED_CHECK();

	if(sscanf(argv[1], "%d", &n) != 1)
		goto usage;

	if(buffers_goto(n))
		gui_status(GUI_ERR, "index %d out of range", n);
}

void cmd_pwd(int argc, char **argv, int force, struct range *rng)
{
	char wd[256];
	if(argc != 1 || force || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s", *argv);
		return;
	}

	if(getcwd(wd, sizeof wd) == NULL)
		gui_status(GUI_ERR, "getcwd(): %s", strerror(errno));
	else
		gui_status(GUI_NONE, "%s", wd);
}

void cmd_cd(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 2 || force || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s dir", *argv);
		return;
	}

	if(chdir(argv[1]))
		gui_status(GUI_ERR, "chdir(): %s", strerror(errno));
}

void cmd_so(int argc, char **argv, int force, struct range *rng)
{
	if(argc != 2 || force || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s file", *argv);
		return;
	}

	gui_term();
	if(!rc_source(argv[1])) /* waits by itself */
		gui_status(GUI_NONE, "sourced %s", argv[1]);
	gui_reload();
}

void cmd_help(int argc, char **argv, int force, struct range *rng)
{
	enum vartype type;

	if(argc != 1 || force || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: %s", *argv);
		return;
	}

	for(type = 0; type != VARS_UNKNOWN; type++)
		vars_help(type);
	gui_status_wait(-1, -1,  NULL);
}

#ifdef BLOAT
# include "bloat/command.c"
#endif

int shellout(const char *cmd, struct list *l)
{
	int ret;

	gui_term();

	if(buffer_modified(buffers_current())){
		puts("uvi: No write since last change");
		fflush(stdout);
	}

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

	for(iter = s + 1; *iter && *iter != ' '; iter++)
		if(*iter == '!'){
			*pforce = 1;
			*iter = ' ';
			break;
		}

	for(iter = strtok(s, " !"); iter; iter = strtok(NULL, " "))
		argv[n++] = iter;

	*pargv = argv;
	*pargc = n;
}

void char_replace(int c, const char *rep, int argc, char **argv)
{
	int i;

	for(i = 0; i < argc; i++)
		argv[i] = chr_expand(argv[i], c, rep);
}

void filter_cmd(int argc, char **argv)
{
	int i;

	/* setup */
	for(i = 0; i < argc; i++)
		argv[i] = ustrdup(argv[i]);


	/* replacements */
	str_home_replace_array(argc, argv);

	if(buffer_hasfilename(buffers_current()))
		char_replace('%', buffer_filename(buffers_current()), argc, argv);
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

		CMD(A),
		CMD(set),
		CMD(yanks),

		CMD(n),
		{ "N", cmd_n },
		CMD(ls),
		CMD(b),

		CMD(cd),
		CMD(pwd),
		CMD(so),

		CMD(help),

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
	lim.end		= buffer_nlines(buffers_current());

	s = parserange(in, &rng, &lim);

	if(!s){
		if(errno == ERANGE){
			if(gui_confirm("range backwards, flip? (y/n) ")){
				int i = rng.start;
				rng.start = rng.end;
				rng.end = i;

				i = strspn(in, "^$.%,-+0123456789");
				s = in + i;
				goto cont;
			}
		}else{
			gui_status(GUI_ERR, "range out of limits");
		}

		return;
	}else if(HAVE_RANGE && *s == '\0'){
		/* just a number, move to that line */
		gui_move(rng.start - 1, gui_x());
		return;
	}

cont:
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
