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
#include "gui/visual.h"
#include "gui/motion.h"
#include "gui/intellisense.h"
#include "gui/gui.h"
#include "util/io.h"
#include "yank.h"
#include "buffers.h"
#include "util/str.h"
#include "rc.h"
#include "gui/map.h"
#include "config.h"
#include "gui/marks.h"

#define LEN(x) ((signed)(sizeof(x) / sizeof(x[0])))

#define MODIFIED_CHECK() \
	if(!force && buffer_modified(buffers_current())){ \
		gui_status(GUI_ERR, "buffer modified%s", buffer_hasfilename(buffers_current()) ? " since last write" : ""); \
		return; \
	}

#define EMPTY_USAGE() \
	if(argc != 1 || force || rng->start != -1 || rng->end != -1){ \
		gui_status(GUI_ERR, "usage: %s", *argv); \
		return; \
	}

void cmd_w(int argc, char **argv, int force, struct range *rng);


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
		gui_term();
		l = pipe_read(cmd);
		gui_reload();

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
	}
}

void cmd_q(int argc, char **argv, int force, struct range *rng)
{
	int qa, xa;

	if(argc != 1 || rng->start != -1 || rng->end != -1){
		gui_status(GUI_ERR, "usage: q[!]");
		return;
	}

	MODIFIED_CHECK();

	qa = !strcmp(*argv, "qa") || (xa = !strcmp(*argv, "xa"));

	if(!qa && !force && buffers_unread()){
		gui_status(GUI_ERR, "unread buffers");
		return;
	}
	if(xa){
		/* save current buffer */
		char *av[] = { "x", NULL };
		cmd_w(1, av, 1, rng);
	}

	global_running = 0;
}

void cmd_w(int argc, char **argv, int force, struct range *rng)
{
#define FINISH() do{ after = NONE; goto after; }while(0)
	extern int gui_statusrestore;
	unsigned int change_mode = 0, old_mode;
	struct list *list_to_write = NULL;
	enum { QUIT, EDIT, NEXT, NONE } after = NONE;
	int nw, nl;
	int x = 0;

	if(rng->start != -1){
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
	}else if(!strcmp(argv[0], "wn")){
		after = NEXT;
		//x = 1;
	}else if(strcmp(argv[0], "w")){
usage:
		gui_status(GUI_ERR, "usage: w[qen][![!]] file|command");
		FINISH();
	}

	if(argc > 1 && argv[1][0] == '!'){
		/* same as above pipe, except the whole file */
		char *cmd = argv_to_str(argc - 1, argv + 1);
		char *bang = strchr(cmd, '!') + 1;

		shellout(bang, buffer_gethead(buffers_current()));

		free(cmd);
		FINISH();
	}

	/* past the point of ! commands */
	if(argc > 2)
		goto usage;

	if(after == NEXT && argc != 1)
		goto usage;

	if(argc == 2 && after != EDIT){
		/* have a filename to save to */
		if(!force){
			struct stat st;

			if(stat(argv[1], &st) == 0){
				gui_status(GUI_ERR, "not over-writing %s", argv[1]);
				FINISH();
			}
		}

		buffer_setfilename(buffers_current(), argv[1]);
		buffer_modified(buffers_current()) = 1;
	}

	if(!buffer_hasfilename(buffers_current())){
		gui_status(GUI_ERR, "buffer has no filename");
		FINISH();
	}

	if(x && !buffer_modified(buffers_current()))
		goto after;

	if(!force){
		if(buffer_readonly(buffers_current())){
			gui_status(GUI_ERR, "buffer is read-only");
			FINISH();
		}
		if(buffer_external_modified(buffers_current())){
			gui_status(GUI_ERR, "buffer changed externally since last read");
			FINISH();
		}
	}

	gui_statusrestore = 0;
	gui_status(GUI_NONE, "\"%s\" ...", buffer_filename(buffers_current()));
	gui_statusrestore = 1;
	gui_refresh();

retry:
	if(change_mode){
		struct stat st;
		const char *fname = buffer_filename(buffers_current());

		if(stat(fname, &st) == 0){
			old_mode = st.st_mode;

			st.st_mode |= 0200;
			if(chmod(fname, st.st_mode)){
				/* try group instead */
				st.st_mode = old_mode | 002;
				if(chmod(fname, st.st_mode)){
					/* try other instead */
					st.st_mode = old_mode | 02;
					chmod(fname, st.st_mode);
					/* the error will show later anyway.. */
				}
			}
		}else{
			change_mode = 0;
		}
		/* else ignore stat error, try again and probably fail */
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
		if(force && errno == EACCES && !change_mode){
			/* attempt with chmod */
			change_mode = 1;
			goto retry;
		}

		gui_status(GUI_ERR, "couldn't write \"%s\": %s", buffer_filename(buffers_current()), strerror(errno));
		after = NONE;
	}else{
		const char *fname = buffer_filename(buffers_current());

		buffer_modified(buffers_current()) = 0;
		gui_status(GUI_NONE, "\"%s\" %s%dL, %dC written",
				buffer_filename(buffers_current()),
				list_to_write ? "[partial-range] ":"",
				nl, nw);

		/* ensure we're on the buffer we just wrote */
		if(!buffers_at_fname(fname))
			buffers_goto(buffers_add(fname));
	}

after:
	if(change_mode){
		/* restore */
		chmod(buffer_filename(buffers_current()), old_mode);
	}

	switch(after){
		case NEXT:
			if(buffers_next(1))
				gui_status(GUI_ERR, "\"%s\" written, no next buffer", buffer_filename(buffers_current()));
			break;
		case EDIT:
			buffers_load(argv[1]);
			break;
		case QUIT:
		{
			char *av[] = { "q", NULL };
			cmd_q(1, av, force, rng);
			break;
		}
		case NONE:
			break;
	}

	if(list_to_write)
		list_free(list_to_write, free);
#undef FINISH
}

void cmd_bang(int argc, char **argv, int force, struct range *rng)
{
	if(rng->start != -1){
		/* rw!cmd command */
		char *free_me = argv_to_str(argc, argv);
		char *cmd = strchr(free_me, '!') + 1;

		if(argc <= 1){
			gui_status(GUI_ERR, "usage: [range]!cmd");
			return;
		}

		if(--rng->start < 0) rng->start = 0;
		if(--rng->end   < 0) rng->end   = 0;

		if(!range_through_pipe(rng, cmd))
			buffer_modified(buffers_current()) = 1;

		free(free_me);
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
	EMPTY_USAGE();

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
		gui_status_wait();
}

void cmd_yanks(int argc, char **argv, int force, struct range *rng)
{
	int one = 0;
	int i;

	EMPTY_USAGE();

	for(i = 'a' - 1; i <= 'z'; i++){
		struct yank *y = yank_get(i);
		char c = i == '`' ? '"': i;

		if(y->v){
			if(!one){
				gui_status_add_start();
				one = 1;
			}

			if(y->is_list)
				gui_status_add(GUI_NONE, "%c: list, length %d, head: \"%s\"", c, list_count(y->v), (const char *)(((struct list *)y->v)->data));
			else
				gui_status_add(GUI_NONE, "%c: string: \"%s\"", c, (const char *)y->v);
		}
	}

	if(one)
		gui_status_wait();
	else
		gui_status(GUI_ERR, "no yanks");
}

void cmd_marks(int argc, char **argv, int force, struct range *rng)
{
	int i, y, x;

	EMPTY_USAGE();

	gui_status_add_start();

#define MARK_SHOW_1(i) \
		if(!mark_get(i, &y, &x)) \
			gui_status_add(GUI_NONE, "'%c' => (%d, %d)", i, x, y) \

#define MARK_SHOW(START, END) \
	for(i = START; i <= END; i++){ \
		MARK_SHOW_1(i); \
	}

	MARK_SHOW('a', 'z')
	MARK_SHOW('0', '9')
	MARK_SHOW_1('\'');
	MARK_SHOW_1('.');

	gui_status_wait();
}

void cmd_maps(int argc, char **argv, int force, struct range *rng)
{
	struct list *map;

	EMPTY_USAGE();

	gui_status_add_start();
	for(map = map_getall(); map; map = map->next){
		struct map *m = map->data;
		gui_status_add(GUI_NONE, "'%c' => \"%s\"", m->c, m->cmd);
	}
	gui_status_wait();
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
		fname = ustrprintf("%s.c",  bfname);

		if(access(fname, F_OK)){
			char *try = ustrprintf("%s.cpp",  bfname);
			if(!access(try, F_OK)){
				free(fname);
				fname = try;
			}else{
				free(try);
			}
		}
	}else{
		gui_status(GUI_ERR, "unknown file extension \"%s\"", ext);
		return;
	}

	buffers_load(fname);
	free(fname);

	return;
}

void cmd_n(int argc, char **argv, int force, struct range *rng)
{
	int i;

	if(argc != 1 || rng->end != rng->start){
		gui_status(GUI_ERR, "usage: [n]%s[!]", *argv);
		return;
	}

	if(rng->start < 0) /* allow :0n to go to 0 */
		rng->start = 1;
	i = rng->start * (**argv == 'n' ? 1 : -1);

	MODIFIED_CHECK();

	if(buffers_next(i))
		gui_status(GUI_ERR, "file index %d %s of buffers", buffers_idx() + i, i > 0 ? "past end" : "before start");
}

void cmd_ls(int argc, char **argv, int force, struct range *rng)
{
	const int cur = buffers_idx();
	struct old_buffer **iter;
	int i;

	EMPTY_USAGE();

	gui_status_add_start();
	gui_status_add(GUI_NONE, "?  #  pos filename");
	for(i = 0, iter = buffers_array(); *iter; iter++, i++)
		gui_status_add(
				i == cur ? GUI_COL_BLUE : GUI_NONE,
				"%c % 2d % 3d %s",
				(*iter)->read ? ' ' : '*',
				i,
				(*iter)->last_y,
				(*iter)->fname);

	gui_status_wait();
}

int ncompar(const void *pa, const void *pb)
{
	int a, b;

	a = *(int *)pa;
	b = *(int *)pb;

	if(a < b)
		return -1;
	if(a > b)
		return 1;
	return 0;
}

void buffer_cmd(int argc, char **argv, int force, struct range *rng, int (*f)(int), int any_count)
{
	int i, n;
	int *bufs, nbufs;

	bufs = NULL;
	nbufs = 0;
	i = 0;

	if(rng->start != -1 || rng->end != -1){
usage:
		gui_status(GUI_ERR, "usage: %s index(s)", *argv);
		return;
	}

	if(any_count){
		if(argc <= 1){
			n = buffers_idx();
			if(n == -1){
				gui_status(GUI_ERR, "no buffer selected");
				return;
			}
			goto perform;
		}
	}else if(argc != 2){
		goto usage;
	}

	for(i = 1; i < argc; i++){
		if(sscanf(argv[i], "%d", &n) != 1)
			goto usage;

		bufs = urealloc(bufs, ++nbufs);
		bufs[i-1] = n;
	}

	qsort(bufs, nbufs, sizeof *bufs, ncompar);

	for(i = nbufs - 1; i >= 0; i--){
		n = bufs[i];
perform:
		if(n == buffers_idx())
			MODIFIED_CHECK();

		if(f(n)){
			gui_status(GUI_ERR, "buffer index %d out of range", n);
			return;
		}
	}

	free(bufs);
}

void cmd_b(int argc, char **argv, int force, struct range *rng)
{
	buffer_cmd(argc, argv, force, rng, buffers_goto, 0);
}

void cmd_bd(int argc, char **argv, int force, struct range *rng)
{
	buffer_cmd(argc, argv, force, rng, buffers_del, 1);
}

void cmd_pwd(int argc, char **argv, int force, struct range *rng)
{
	char wd[256];
	EMPTY_USAGE();

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

void cmd_noh(int argc, char **argv, int force, struct range *rng)
{
    extern char *search_str;
	EMPTY_USAGE();

    if(search_str)
        *search_str = '\0';
}

void cmd_help(int argc, char **argv, int force, struct range *rng)
{
	enum vartype type;

	EMPTY_USAGE();

	gui_status_add_start();
	for(type = 0; type != VARS_UNKNOWN; type++)
		vars_help(type);

	gui_status_add(GUI_NONE, "");
	gui_status_add(GUI_NONE, "useful commands (see uvi(1) for all)");
	gui_status_add(GUI_NONE, ":mak");
	gui_status_add(GUI_NONE, ":set");
	gui_status_add(GUI_NONE, ":marks");
	gui_status_add(GUI_NONE, ":yanks");
	gui_status_add(GUI_NONE, ":{ls,[nN],b,bd}");

	gui_status_wait();
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
	}else{
		ret = system(cmd);
	}

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

void parse_cmd(char *s, int *pargc, char ***pargv, int *pforce)
{
#define argv (*pargv)
#define argc (*pargc)

	char *p, *mark;
	int fin = 0;
	int initial = 0;
	int in_quote = 0;

	argv = NULL;
	argc = 0;

	if(*s == '!'){
		/* :!... */
		argv    = umalloc((argc = 2) * sizeof *argv);
		argv[0] = ustrdup("!");
		argv[1] = ustrdup(s + 1);
		return;
	}

	/* search for first non-alnum() char, the fist time we do the loop, for :s/...  */

	for(p = mark = s; !fin; p++)
		switch(*p){
			case '\'':
				in_quote = !in_quote;
				continue;
			case '\\':
				if(!in_quote)
					memmove(p, p + 1, strlen(p)); /* remove the escape char */
				continue;
			case ' ':
			case '\t':
				if(in_quote)
					continue;
			case '\0':
sep:
				initial = 1;
				argv = urealloc(argv, ++argc * sizeof *argv);
				argv[argc-1] = ustrdup2(mark, p);

				/* increase p until we're at the next non-space */
				while(*p && isspace(*p))
					p++;

				if(!*p)
					fin = 1;
				else
					mark = p--;

				break;

			default:
				if(!initial && !isalnum(*p)){
					/* assume the initial non-alnum char is a separator too - :s/a/b/c... */
					if(*p == '!'){
						*p = ' ';
						*pforce = 1;
					}
					goto sep;
				}
		}

	if(argc > 0){
		int len = strlen(argv[0]);
		if(argv[0][len - 1] == '!'){
			argv[0][len - 1] = '\0';
			*pforce = 1;
		}
	}

#undef argv
#undef argc
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

	if(buffers_alternate())
		char_replace('#', buffers_alternate(), argc, argv);
}

void command_run(char *in)
{
	static const struct
	{
		const char *nam;
		void (*f)(int argc, char **argv, int force, struct range *rng);
		int changes, changes_with_range;
	} funcs[] = {
#define CMD(x, c) { #x, cmd_##x, c, 0 }
		{ "!",  cmd_bang,  0, 1 },
		{ "?",  cmd_where, 0, 0 },

		{ "we", cmd_w, 1, 0 },
		{ "wq", cmd_w, 1, 0 },
		{ "wn", cmd_w, 1, 0 },
		{ "x",  cmd_w, 1, 0 },
		CMD(new, 0),
		CMD(r,   1),
		CMD(w,   1),
		CMD(q,   0),
		CMD(e,   0),
		{ "qa", cmd_q, 0, 0 },
		{ "xa", cmd_q, 1, 0 },

		CMD(A,     0),
		CMD(set,   0),
		CMD(yanks, 0),
		CMD(maps,  0),
		CMD(marks, 0),

		CMD(n, 0),
		{ "N", cmd_n, 0, 0 },
		CMD(ls, 0),
		CMD(b,  0),
		CMD(bd, 0),

		CMD(cd,  0),
		CMD(pwd, 0),
		CMD(so,  0),

		CMD(noh, 0),

		CMD(help, 0),

#ifdef BLOAT
# include "bloat/command.h"
#endif

#undef CMD
	};

#define HAVE_RANGE() (s > in)
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
	}else if(HAVE_RANGE() && *s == '\0'){
		/* just a number, move to that line */
		gui_move(rng.start - 1, gui_x());
		return;
	}

cont:
	if(!HAVE_RANGE())
		rng.start = rng.end = -1;

	parse_cmd(s, &argc, &argv, &force);
	filter_cmd(argc, argv);

	found = 0;
	for(i = 0; i < LEN(funcs); i++)
		if(!strcmp(argv[0], funcs[i].nam)){
			found = 1;
			if((funcs[i].changes || (rng.start >= 0 && funcs[i].changes_with_range))
				 && buffer_readonly(buffers_current())){
				gui_status(GUI_ERR, "command modifies, and buffer is readonly");
				break;
			}
			funcs[i].f(argc, argv, force, &rng);
			break;
		}

	if(!found)
		gui_status(GUI_ERR, "\":%s\": not an editor command (\"%s\")", argv[0], s);

	for(i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}
