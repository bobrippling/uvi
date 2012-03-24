#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "../range.h"
#include "../buffer.h"
#include "../command.h"
#include "../util/list.h"
#include "../global.h"
#include "visual.h"
#include "motion.h"
#include "../util/alloc.h"
#include "intellisense.h"
#include "gui.h"
#include "macro.h"
#include "marks.h"
#include "../main.h"
#include "intellisense.h"
#include "../str/str.h"
#include "../yank.h"
#include "map.h"
#include "../buffers.h"
#include "../util/search.h"
#include "extra.h"

#define REPEAT_FUNC(nam) static void nam(unsigned int)

/* text altering */
static void open(int); /* as in, 'o' & 'O' */
static void shift(int);
static void insert(int append, int do_indent, int trim_initial);
static void put(unsigned int ntimes, int rev);
static void change(struct motion *motion, int flag);
REPEAT_FUNC(join);
REPEAT_FUNC(replace);

/* extra */
static void colon(const char *);
static int  search(int, int);

static int is_edit_char(int);

static void showpos(void);


char *search_str = NULL;
static int  search_rev  = 0;
static int  yank_char = 0;

static int search(int next, int rev)
{
	const char *(*usearch_func)(struct usearch *, const char *, int);
	struct list *l;
	const char *p;
	int y;
	int found;
	int offset, setoffset;
	struct usearch us;

	if(next){
		if(!search_str || !*search_str){
			gui_status(GUI_ERR, "no previous search");
			return 1;
		}
		rev = rev - search_rev; /* obey the previous "?" or "/" */
	}else{
		struct gui_read_opts opts;
		search_rev = rev;
		intellisense_init_opt(&opts, INTELLI_NONE);
		if(gui_prompt(rev ? "?" : "/", &search_str, &opts))
			return 1;
	}

	if(!*search_str)
		return 1;

	found = 0;
	if(usearch_init(&us, search_str)){
		gui_status(GUI_ERR, "regex error %s", usearch_err(&us));
		goto fin;
	}

	mark_jump();

	/* TODO: allow SIGINT to stop search? */
	usearch_func = rev ? usearch_rev : usearch;

	offset = gui_x() + (rev ? -1 : 1);
	if(offset < 0) offset = 0;
	setoffset = 0;

	y = gui_y();
	for(
			l = buffer_getindex(buffers_current(), y);
			l;
			rev ? (y--, l = l->prev) : (y++, l = l->next)
			){

		if(setoffset)
			offset = rev ? strlen(l->data) : 0;

		if((p = usearch_func(&us, (const char *)l->data, offset))){
			int x = p - (char *)l->data;
			found = 1;
			gui_move(y, x);
			gui_status(GUI_NONE, "y=%d x=%d", y, x);
			break;
		}

		setoffset = 1;
	}

	if(!found){
		gui_status(GUI_ERR, "/%s/ not found %s", search_str, rev ? "above" : "below");
		gui_move(gui_y(), gui_x());
	}

fin:
	usearch_free(&us);

	return !found;
}

void shiftline(char **ps, int indent)
{
	char *s = *ps;

	if(line_isspace(s)){
		*s = '\0';
		return;
	}

	if(indent > 0){
		int len;

		if(global_settings.et)
			indent *= global_settings.tabstop;

		len = strlen(s) + indent;
		s = urealloc(s, len + 1);

		memmove(s + indent, s, len - indent + 1);
		memset(s, global_settings.et ? ' ' : '\t', indent);

		*ps = s;
	}else{
		while(indent++ < 0)
			switch(*s){
				case ' ':
				{
					/* ensure we can unindent by global_settings.tabstop spaces */
					int i, spaced = 1;
					for(i = 0; i < global_settings.tabstop; i++)
						if(s[i] != ' '){
							spaced = 0;
							break;
						}

					if(spaced)
						memmove(s, s + global_settings.tabstop, strlen(s + global_settings.tabstop) + 1);
					else
						goto fin;
					break;
				}

				case '\t':
					memmove(s, s+1, strlen(s));
					break;

				default:
					goto fin;
			}
	}
fin:;
}

void shift(int repeat)
{
	struct list *l;
	int x1, y1, x2, y2;

	/* FIXME? should be MOTION_BACKWARD_LETTER if '<' ? */
	if(motion_wrap(&x1, &y1, &x2, &y2, "><", MOTION_FORWARD_LETTER))
		return;

	l = buffer_getindex(buffers_current(), y1);
	while(y1++ <= y2 && l){
		shiftline((char **)&l->data, repeat);
		l = l->next;
	}

	gui_move_sol(y1 - 1);
	buffer_modified(buffers_current()) = 1;
}

void replace(unsigned int n)
{
	int c;
	struct list *cur = buffer_getindex(buffers_current(), gui_y());
	char *s = cur->data;

	if(!*s)
		return;

	if(n <= 0)
		n = 1;
	else if(n > strlen(s + gui_x()))
		return;

	c = gui_getch(GETCH_RAW);

	if(c == '\n'){
		/* delete n chars, and insert 1 line */
		char *off = s + gui_x() + n-1;
		char *cpy = umalloc(strlen(off));

		memset(off - n + 1, '\0', n);
		strcpy(cpy, off + 1);

		buffer_insertafter(buffers_current(), cur, cpy);

		gui_move(gui_y() + 1, 0);
	}else if(c != CTRL_AND('[')){
		int x = gui_x();

		while(n--)
			s[x + n] = c;
	}

	buffer_modified(buffers_current()) = 1;
}

static void showpos()
{
	const int i = buffer_nlines(buffers_current());

	gui_status(GUI_NONE, "%s%s%s [%c] %d/%d %.2f%%",
			buffer_hasfilename(buffers_current()) ? "\"" : "",
			buffer_filename(buffers_current()),
			buffer_hasfilename(buffers_current()) ? "\"" : "",
			"+-"[!buffer_modified(buffers_current())],
			1 + gui_y(), i,
			100.0f * (float)(1 + gui_y()) /(float)i);
}

static int findindent(const char *s)
{
	int tabs, spc;

	tabs = spc = 0;

	while(*s)
		switch(*s++){
			case '\t':
				tabs++;
				break;
			case ' ':
				spc++;
				break;
			default:
				goto lewpfin;
		}

lewpfin:
	if(global_settings.et)
		return spc + tabs * global_settings.tabstop;;
	return tabs + spc / global_settings.tabstop;;
}

void readlines(int do_indent, int can_trim_initial, struct gui_read_opts *opts, char ***plines, int *pi)
{
#define INDENT_ADJ global_settings.et ? global_settings.tabstop : 1
	int nlines;
	char **lines;
	int indent = 0;
	int i = 0;

	nlines = 10;
	lines = umalloc(nlines * sizeof *lines);

	if(do_indent && global_settings.autoindent){
		struct list *lprev = buffer_getindex(buffers_current(), gui_y() - 1);
		int first_indent;

		first_indent = indent = 0;

		for(; lprev; lprev = lprev->prev)
			if(!line_isspace(lprev->data)){
				indent = findindent(lprev->data);
				break;
			}else if(!first_indent){
				first_indent = findindent(lprev->data);
			}

		if(!indent)
			indent = first_indent;
	}

	for(;;){
		int esc;
		int saveindent = indent;

		while(indent --> 0)
			gui_ungetch(global_settings.et ? ' ' : '\t');

		if(global_settings.cindent && i > 0){
			char *p = lines[i-1];;

			for(; *p && isspace(*p); p++);

			if(*p == '*')
				/* c-commenting */
				gui_queue(" *");
		}

		lines[i] = NULL;
		esc = gui_getstr(&lines[i], opts);
		if(++i >= nlines)
			lines = urealloc(lines, (nlines += 10) * sizeof *lines);

		if(do_indent){
			/*
			* trim the line if
			* a) it's just spaces
			* and
			* b) it's a new line or it's an empty line insert
			*/
			if((i > 1 || can_trim_initial) && line_isspace(lines[i-1])){
				*lines[i-1] = '\0';
				indent = saveindent;
			}else{
				indent = findindent(lines[i-1]);
			}
		}

		if(global_settings.cindent){
			char *s;

			s = lines[i-1];

			if(*s && s[strlen(s)-1] == '{')
				indent += INDENT_ADJ;

			for(s = lines[i-1]; *s; s++)
				if(!isspace(*s)){
					if(*s == '}'){
						/* need to unindent by one */
						indent -= INDENT_ADJ;
						shiftline(&lines[i-1], -1);
					}
					break;
				}
		}


		/* must be last, to trim, etc, before returning */
		if(esc)
			/* ^[ */
			break;
		gui_clrtoeol();
	}

	*plines = lines;
	*pi = i;
#undef INDENT_ADJ
}

static void trim_buffer(void)
{
	buffer_t *cb = buffers_current();
	struct list *l;

	buffer_modified(cb) = 1;

	for(l = buffer_gethead(cb); l; l = l->next)
		str_rtrim(l->data);
}

static void insert(int append, int do_indent, int trim_initial)
{
	char **lines;
	struct gui_read_opts opts;
	int x = gui_x();
	int i;

	intellisense_init_opt(&opts, INTELLI_INS);

	if(append){
		x++;
		gui_inc_cursor();
	}

	readlines(do_indent, trim_initial, &opts, &lines, &i);

	{
		struct list *iter = buffer_getindex(buffers_current(), gui_y());
		char *ins;
		char *after;

		ins = (char *)iter->data + x;
		after = ustrdup(ins);
		*ins = '\0';

		if(i > 1){
			/* add all lines, then join with v_after */
			int j;

			ustrcat((char **)&iter->data, NULL, *lines, NULL);

			/* tag v_after onto the last line */
			ustrcat(&lines[i-1], NULL, after, NULL);

			for(j = i - 1; j > 0; j--)
				buffer_insertafter(buffers_current(), iter, lines[j]);

			gui_move(gui_y() + i - 1, strlen(after) + strlen(lines[i-1]));
		}else{
			/* tag v_after on the end */
			ustrcat((char **)&iter->data, NULL, *lines, after, NULL);
			gui_move(gui_y(), gui_x() + strlen(*lines) - !append); /* if append, no need to hopback */
		}
		free(*lines);
		free(after);
	}

	buffer_modified(buffers_current()) = 1;
	free(lines);

	if(global_settings.esctrim)
		trim_buffer();
}

void overwrite()
{
	struct gui_read_opts opts;
	struct list *iter;
	int start_y, start_x;
	char **lines;
	int nl;
	int i;

	intellisense_init_opt(&opts, INTELLI_INS);

	start_y = gui_y();
	start_x = gui_x();

	iter = buffer_getindex(buffers_current(), start_y);

	readlines(0 /* indent */, 0, &opts, &lines, &nl);

	if(strlen(*lines) > strlen(iter->data))
		/* need to extend iter->data */
		iter->data = urealloc(iter->data, strlen(iter->data) + strlen(*lines) + 1 /* plenty */);

	/* don't copy the nul byte */
	strncpy((char *)iter->data + start_x, *lines, strlen(*lines));

	/* FIXME? if nl>0, instead of discarding *(iter->data + startx + strlen(*lines))..., tag it onto the end? */

	/* tag all other lines onto the end */
	for(i = nl - 1; i > 0; i--)
		buffer_insertafter(buffers_current(), iter, lines[i]);

	gui_move(start_y + nl - 1, nl > 1 ? strlen(lines[nl-1]) : start_x + strlen(*lines) - 1);
	buffer_modified(buffers_current()) = 1;
}

static void open(int before)
{
	struct list *here;

	here = buffer_getindex(buffers_current(), gui_y());

	if(before){
		buffer_insertbefore(buffers_current(), here, ustrdup(""));
		gui_move(gui_y(), 0);
	}else{
		buffer_insertafter(buffers_current(), here, ustrdup(""));
		gui_move(gui_y() + 1, gui_x());
	}

	gui_clrtoeol();
	insert(0, 1, 1);

#if 0
	if(!before)
		gui_move(gui_y() - 1, gui_x());
#endif
}

static void motion_cmd(struct motion *motion,
		void (*f_line )(struct range *),
		void (*f_range)(char *data, int startx, int endx)
		)
{
	struct bufferpos topos;
	struct screeninfo si;
	int x = gui_x(), y = gui_y();

	topos.x = &x;
	topos.y = &y;
	si.top = gui_top();
	si.height = gui_max_y();

	if(!motion_apply(motion, &topos, &si)){
		struct range from;

		from.start = gui_y();
		from.end   = y;

		if(from.start > from.end){
			int t = from.start;
			from.start = from.end;
			from.end = t;
		}

		if(motion->visual_state == VISUAL_BLOCK){
			/* f_line for each line in range */
#define ystart  from.start
#define yend    y
#define xstart2 from.end
#define xend2   x
			struct list *lp;
			int xstart, xend;

			if(xend2 > xstart2){
				xstart = xstart2;
				xend   = xend2;
			}else{
				xstart = xend2;
				xend   = xstart2;
			}

			/* FIXME: doesn't work for yank, needs new structure or something */
			for(ystart = from.start, lp = buffer_getindex(buffers_current(), ystart);
					ystart <= yend;
					ystart++, lp = lp->next)
				f_range(lp->data, xstart, xend);
#undef ystart
#undef yend
#undef xstart2
#undef xend2

		}else{
			if(motion_is_line(motion)){
				/* delete lines between gui_y() and y, inclusive */
				f_line(&from);
				gui_move_sol(from.start);
			}else{
				char *data = buffer_getindex(buffers_current(), gui_y())->data;
				int startx = gui_x();

				if(from.start < from.end){
					/* there are also lines to remove */
					f_line(&from);
				}else{
					/* startx should be left-most */
					if(startx > x){
						int t = x;
						x = startx;
						startx = t;
					}

					switch(motion->motion){
						case MOTION_FIND:
						case MOTION_TIL:
						case MOTION_FIND_REV:
						case MOTION_TIL_REV:
						case MOTION_ABSOLUTE_RIGHT:
						case MOTION_PAREN_MATCH:
							x++;
						default:
							break;
					}

					f_range(data, startx, x);
				}

				gui_move(gui_y(), startx);
			}
		}
	}
}

static void delete_line(struct range *from)
{
	struct list *l = buffer_extract_range(buffers_current(), from);
	yank_set_list(yank_char, l);
	gui_move(gui_y(), gui_x());
	buffer_modified(buffers_current()) = 1;
}
static void delete_range(char *data, int startx, int x)
{
	int len = x - startx;
	char *dup = umalloc(len + 1);
	strncpy(dup, data + startx, len);
	dup[len] = '\0';
	yank_set_str(yank_char, dup);

	/* remove the chars between startx and x, inclusive */
	if(len == 0)
		x++; /* fix for 'x' at eol */
	memmove(data + startx, data + x, strlen(data + x) + 1);
	buffer_modified(buffers_current()) = 1;
}

static void yank_line(struct range *from)
{
	yank_set_list(yank_char, buffer_copy_range(buffers_current(), from));
}
static void yank_range(char *data, int startx, int x)
{
	int len = x - startx;
	char *dup = umalloc(len + 1);

	strncpy(dup, data + startx, x - startx);
	dup[len] = '\0';

	yank_set_str(yank_char, dup);
}

static void change(struct motion *motion, int ins)
{
	int x, y, dollar = 0;

	if(motion_get(motion, 1, 0, "dc", MOTION_WHOLE_LINE))
		return;

	if(ins){
		struct bufferpos pos;
		struct screeninfo si;

		x = gui_x();
		y = gui_y();

		si.top = gui_top();
		si.height = gui_max_y();
		pos.x = &x;
		pos.y = &y;

		if(!motion_apply(motion, &pos, &si))
			dollar = 1;
	}

	motion_cmd(motion, delete_line, delete_range);

	if(ins){
		if(visual_get() == VISUAL_BLOCK){
			/* TODO */
		}

		if(dollar)
			gui_mvaddch(y, x > 0 ? x - 1 : x, '$');
		gui_move(gui_y(), gui_x());
		insert(0, 0, 0);
	}
}

static void put(unsigned int ntimes, int rev)
{
	struct yank *ynk = yank_get(yank_char);

	if(!ynk->v){
		gui_status(GUI_ERR, "register %c empty", yank_char ? yank_char : '"');
		return;
	}


	if(ynk->is_list){
#define INS(f) \
		f( \
			buffers_current(), \
			buffer_getindex(buffers_current(), gui_y()), \
			list_copy(ynk->v, (void *(*)(void *))ustrdup) \
		)

		if(rev)
			INS(buffer_insertlistbefore);
		else
			INS(buffer_insertlistafter);

		gui_move(gui_y() + (rev ? 0 : list_count(ynk->v)), gui_x());

	}else{
		struct list *l = buffer_getindex(buffers_current(), gui_y());
		const int x = gui_x() + 1 - rev;
		char *data = l->data;
		char *after = ALLOCA(strlen(data + x) + 1);
		char *new;

		strcpy(after, data + x);
		data[x] = '\0';

		new = ustrprintf("%s%s%s", data, (char *)ynk->v, after);

		free(l->data);
		l->data = new;

		gui_move(gui_y(), x + strlen(ynk->v) - 1);
	}

	buffer_modified(buffers_current()) = 1;
}

static void join(unsigned int ntimes)
{
	struct list *jointhese, *l, *cur;
	struct range r;
	int len, initial_len;

	{
		unsigned int nl = buffer_nlines(buffers_current());
		if(nl <= 1 || gui_y() + ntimes >= nl - 1){
			gui_status(GUI_ERR, "can't join %d line%s", ntimes,
					ntimes == 1 ? "" : "s");
			return;
		}
	}

	cur = buffer_getindex(buffers_current(), gui_y());

	r.start = gui_y() + 1; /* extract the next line(s) */
	r.end   = r.start + ntimes;

	jointhese = buffer_extract_range(buffers_current(), &r);

	len = 0;
	for(l = jointhese; l; l = l->next){
		str_trim(l->data);
		len += strlen(l->data) + (*(char *)l->data ? 1 : 0);
	}

	initial_len = strlen(cur->data);
	cur->data = urealloc(cur->data, initial_len + len + 1);

	for(l = jointhese; l; l = l->next){
		if(*(char *)cur->data)
			strcat(cur->data, " ");
		strcat(cur->data, l->data);
	}

	list_free(jointhese, free);

	gui_move(gui_y(), initial_len);
	buffer_modified(buffers_current()) = 1;
}

static void colon(const char *initial)
{
	struct gui_read_opts opts;
	char *in = NULL;

	if(initial && *initial)
		gui_queue(initial);

	intellisense_init_opt(&opts, INTELLI_CMD);

	if(!gui_prompt(":", &in, &opts)){
		char *c = strchr(in, '\n');

		if(c)
			*c = '\0';

		command_run(in);
	}else{
		gui_status(GUI_NONE, "");
	}

	/*visual_set(VISUAL_NONE);*/

	free(in);
}

void visual_inside(int include)
{
	struct list *l;
	char *s;
	int i;
	int start_y;
	int bracket;

	/* TODO: number */

	bracket = gui_getch(GETCH_COOKED);
	l = buffer_getindex(buffers_current(), gui_y());
	s = l->data;

	if(!isparen(bracket))
		/* TODO: iw, ip (paragraph), etc - swap to a motion? */
		return;

	/* TODO: go forward if it's a close bracket, else go backward */

	i = gui_x();
	start_y = gui_y();
	while(s[i] != bracket){
		if(--i < 0){
			l = l->prev;
			start_y--;
			if(!l)
				return;
			s = l->data;
			i = strlen(s) - 1;
		}
	}

	/* s[i] is the bracket, do a motion_percent on it and visual() */
	{
		/* TODO: distinguish between vi{ and va{ */
		struct motion m;
		struct bufferpos bp;
		struct screeninfo si;
		int x[2], y[2];

		memset(&m, 0, sizeof m);
		m.motion = MOTION_PAREN_MATCH;

		si.top = gui_top();
		si.height = gui_max_y();

		x[0] = x[1] = i;
		y[0] = y[1] = start_y;
		bp.x = x + 1;
		bp.y = y + 1;

		if(motion_apply(&m, &bp, &si))
			return;

		/* highlight from {x,y][1] to {x,y}[0] */
		if(!include){
			int top, bottom;

			if(y[0] > y[1]){
				top    = 1;
				bottom = 0;
			}else{
				bottom = 1;
				top    = 0;
			}

			if(x[bottom] > 0)
				x[bottom]--;
			else
				y[bottom]--;

			if(x[top] > 0)
				x[top]++;
			else
				y[top]++;
			/* FIXME */
		}

		visual_setpoints(x, y);
	}
}

static int is_edit_char(int c)
{
	switch(c){
		case 'o':
		case 'O':
			if(visual_get() != VISUAL_NONE)
				break;
		case 'X':
		case 'x':
		case 'C':
		case 'D':
		case 'c':
		case 'd':
		case 'A':
		case 'a':
		case 'i':
		case 'I':
		case 'J':
		case 'r':
		case '>':
		case '<':
		case '~':
		case 'p':
		case 'P':
			return 1;
	}
	return 0;
}

void gui_show_if_modified()
{
	buffer_t *const cb = buffers_current();

	if(buffer_external_modified(cb))
		gui_status(GUI_ERR, "buffer externally modified");
	else if(buffer_touched_filesystem(cb) && !buffer_file_exists(cb))
		gui_status(GUI_ERR, "buffer's file doesn't exist");
}

void gui_run()
{
	extern int gui_scrollclear;
	struct motion motion;
	int buffer_changed;
	int view_changed;
	int prevcmd;
	int multiple, prevmultiple;

	buffer_changed = 0;
	view_changed = 1;
	prevcmd = 0;
	prevmultiple = multiple = 0;
	gui_scrollclear = 1; /* already show "opened xyz.txt" ..., ok to clear */

	do{
		int flag = 0, resetmultiple = 1;
		int c;

		if(buffer_changed){
			buffer_changed = 0;
			view_changed = 1;
		}
		if(view_changed){
			if(gui_peekunget() != ':')
				gui_draw();
			view_changed = 0;
		}

#define INC_MULTIPLE() \
				do \
					if(multiple < INT_MAX/10){ \
						multiple = multiple * 10 + c - '0'; \
						resetmultiple = 0; \
					}else \
						gui_status(GUI_ERR, "range too large"); \
						/*resetmultiple = 1;*/ \
				while(0)

#define SET_DOT() do{ \
					prevcmd = c; \
					prevmultiple = multiple; \
				}while(0)

#define SET_MOTION(m) do{ \
					motion.motion = m; \
					motion.ntimes = multiple; \
				}while(0)

switch_start:
		yank_char = 0;
		c = gui_getch(GETCH_MEDIUM_RARE);
		if(is_edit_char(c)){
			if(buffer_readonly(buffers_current())){
				gui_status(GUI_ERR, "buffer is read-only");
				continue;
			}
			/*
			else if(visual_get() != VISUAL_NONE){
				visual_set(VISUAL_NONE);
			}
			- this should only be done for commands that insert, such as c<motion>
			*/

			mark_edit();
		}

switch_switch:
		switch(c){
			/*
			 * TODO: different switch when in visual mode?
			 * struct cmd
			 * {
			 *   char ch;
			 *   void (*f)();
			 *   int when_visual;
			 * };
			 */
			case '!':
				if(visual_get() == VISUAL_NONE){
					int ch = gui_getch(GETCH_COOKED);
					switch(ch){
						case 'f':
							if(!go_file())
								buffer_changed = 1;
							break;
						case 'q':
							if(!fmt_motion())
								buffer_changed = 1;
							break;
						default:
							gui_status(GUI_ERR, "Invalid ! suffix");
					}
					break;
				}
				/* else fall */

			case ':':
			{
				char buffer[16];
				if(visual_get() != VISUAL_NONE)
					snprintf(buffer, sizeof buffer, "%d,%d%s",
							visual_get_start()->start + 1, /* convert to 1-based */
							visual_get_end(  )->start + 1,
							c == '!' ? "!" : "");
				else
					*buffer = '\0';

				colon(buffer);

				/* need to view_refresh_or_whatever() */
				buffer_changed = 1;
				break;
			}

			case '.':
				if(prevcmd){
					gui_ungetch(prevcmd);
					multiple = prevmultiple;
					goto switch_start;
				}else
					gui_status(GUI_ERR, "no previous command");
				break;

			case 'm':
				c = gui_getch(GETCH_COOKED);
				if(mark_valid(c)){
					mark_set(c, gui_y(), gui_x());
					gui_status(GUI_NONE, "'%c' => (%d, %d)", c, gui_x(), gui_y());
				}else{
					gui_status(GUI_ERR, "invalid mark");
				}
				break;

			case CTRL_AND('g'):
				showpos();
				view_changed = 1;
				break;

			case 'O':
				flag = 1;
			case 'o':
				if(visual_get() != VISUAL_NONE){
					if(flag)
						visual_join();
					else
						visual_swap();
					view_changed = 1;
				}else{
					open(flag);
					buffer_changed = 1;
					SET_DOT();
				}
				break;

			case 's':
				flag = 1;
			case 'X':
			case 'x':
				SET_MOTION(c == 'X' ? MOTION_BACKWARD_LETTER : MOTION_FORWARD_LETTER);
				motion_cmd(&motion, delete_line, delete_range);
				buffer_changed = 1;
				SET_DOT();
				if(flag)
					insert(0, 0, 0);
				break;

			case 'C':
				flag = 1;
			case 'D':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				motion_cmd(&motion, delete_line, delete_range);
				if(flag)
					insert(1 /* append */, 0, 0);
				buffer_changed = 1;
				SET_DOT();
				break;

			case 'S':
				gui_ungetch('c'); /* FIXME? */
			case 'c':
				flag = 1;
			case 'd':
				motion.ntimes = multiple;
				change(&motion, flag);
				view_changed = 1;
				buffer_changed = 1;
				SET_DOT();
				break;

			case '"':
				yank_char = gui_getch(GETCH_COOKED);
				if(!yank_char_valid(yank_char)){
					yank_char = 0;
					break;
				}
				c = gui_getch(GETCH_COOKED);
				goto switch_switch;

			case 'P':
				flag = 1;
			case 'p':
				put(multiple, flag);
				buffer_changed = 1;
				break;

			case 'y':
				if(motion_get(&motion, 1, multiple, "y", MOTION_WHOLE_LINE))
					break;
				motion_cmd(&motion, yank_line, yank_range);
				SET_DOT();
				view_changed = 1;
				break;

			case 'A':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				gui_move_motion(&motion);
			case 'a':
				flag = 1;
			case 'i':
case_i:
				if(visual_get() == VISUAL_NONE){
					insert(flag, 0, 0);
					buffer_changed = 1;
					SET_DOT();
				}else{
					/* ibracket */
					visual_inside(flag);
					view_changed = 1;
				}
				break;
			case 'I':
				SET_MOTION(MOTION_LINE_START);
				gui_move_motion(&motion);
				goto case_i;

			case 'R':
				overwrite();
				buffer_changed = 1;
				break;

			case 'J':
				join(multiple);
				buffer_changed = 1;
				SET_DOT();
				break;

			case 'r':
				replace(multiple);
				buffer_changed = 1;
				SET_DOT();
				break;

			case CTRL_AND('f'):
				view_changed = gui_scroll(PAGE_DOWN);
				break;
			case CTRL_AND('b'):
				view_changed = gui_scroll(PAGE_UP);
				break;
			case CTRL_AND('d'):
				view_changed = gui_scroll(HALF_DOWN);
				break;
			case CTRL_AND('u'):
				view_changed = gui_scroll(HALF_UP);
				break;
			case CTRL_AND('e'):
				view_changed = gui_scroll(SINGLE_DOWN);
				break;
			case CTRL_AND('y'):
				view_changed = gui_scroll(SINGLE_UP);
				break;

			case CTRL_AND('l'):
				gui_status(GUI_NONE, "");
				gui_draw();
				break;

			case '*':
			case '#':
			case 'n':
			case 'N':
			case '/':
			case '?':
			{
				int rev;
				int next;

				if(c == '*' || c == '#'){
					char *w = gui_current_word();

					if(!w){
						gui_status(GUI_ERR, "no word selected");
						break;
					}

					search_rev = c == '#';
					rev = 0; /* force the right direction */
					next = 1;

					if(search_str)
						free(search_str);

					search_str = w;
				}else{
					rev  = c == '?' || c == 'N';
					next = tolower(c) == 'n';
				}

				view_changed = 1;
				search(next, rev);
				break;
			}

#define DO_INDENT(sign) \
				if(!multiple) multiple = 1; \
				shift(sign multiple); \
				buffer_changed = 1; \
				SET_DOT()

			case '>':
				DO_INDENT(+);
				break;
			case '<':
				DO_INDENT(-);
				break;

			case '~':
				tilde(multiple);
				SET_DOT();
				buffer_changed = 1;
				break;

			case 'K':
				showgirl(multiple);
				break;

			case 'z':
				/* screen move - vim's zz, zt & zb */
				switch(gui_getch(GETCH_COOKED)){
					case 'z':
						gui_scroll(CURSOR_MIDDLE);
						break;
					case 't':
						gui_scroll(CURSOR_TOP);
						break;
					case 'b':
						gui_scroll(CURSOR_BOTTOM);
						break;
				}
				view_changed = 1;
				break;

			case CTRL_AND('['):
				visual_set(VISUAL_NONE);
				view_changed = 1;
				break;

			case '\\':
				map();
				break;

			case CTRL_AND('v'):
			case 'V':
			{
				enum visual target = c == 'V' ? VISUAL_LINE : VISUAL_BLOCK;
				if(visual_get() != target){
					visual_set(target);
					visual_status();
				}else{
					visual_set(VISUAL_NONE);
				}
				view_changed = 1;
				break;
			}

			case 'Z':
			{
				char buf[4];
				c = gui_getch(GETCH_COOKED);
#define MAP(c, cmd) case c: strcpy(buf, cmd); command_run(buf); break
				switch(c){
					MAP('Z', "x");
					MAP('Q', "q!");
					MAP('W', "w");
#undef MAP

					default:
						gui_status(GUI_ERR, "unknown Z suffix", c);
				}
				break;
			}

			case 'q':
				if(gui_macro_recording()){
					gui_status(GUI_NONE, "recorded to %c", gui_macro_complete());
				}else{
					int m = gui_getch(GETCH_COOKED);
					if(macro_char_valid(m)){
						gui_status(GUI_COL_MAGENTA, "recording (%c)", m);
						gui_macro_record(m);
					}
				}
				break;
			case '@':
			{
				int m = gui_getch(GETCH_COOKED);
				if(macro_char_valid(m)){
					if(!multiple)
						multiple = 1;
					while(multiple --> 0)
						macro_play(m);
				}
				break;
			}

			default:
				if(isdigit(c) && (c == '0' ? multiple : 1)){
					INC_MULTIPLE();
				}else{
					gui_ungetch(c);
					if(!motion_get(&motion, 0, multiple, "", 0)){
						if(motion_is_big(&motion) && motion.motion != MOTION_MARK)
							mark_jump();
						gui_move_motion(&motion);
						if(visual_get() != VISUAL_NONE)
							visual_status();
						view_changed = 1;
					}
				}
		}

		if(resetmultiple)
			multiple = 0;
	}while(global_running);
#undef INC_MULTIPLE
#undef SET_DOT
}
