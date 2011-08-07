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
#include "motion.h"
#include "../util/alloc.h"
#include "intellisense.h"
#include "gui.h"
#include "macro.h"
#include "marks.h"
#include "../main.h"
#include "intellisense.h"
#include "../util/str.h"
#include "../yank.h"
#include "map.h"
#include "../buffers.h"
#include "visual.h"

#define REPEAT_FUNC(nam) static void nam(unsigned int)

/* text altering */
static void open(int); /* as in, 'o' & 'O' */
static void shift(unsigned int, int);
static void insert(int append, int indent);
static void put(unsigned int ntimes, int rev);
static void change(struct motion *motion, int flag);
REPEAT_FUNC(join);
REPEAT_FUNC(tilde);
REPEAT_FUNC(replace);

/* extra */
static void colon(const char *);
static int  search(int, int);
REPEAT_FUNC(showgirl);

static int is_edit_char(int);

static void showpos(void);


char *search_str = NULL;
static int  search_rev  = 0;
static int  yank_char = 0;

static int search(int next, int rev)
{
	struct list *l;
	char *p;
	int y;
	int found = 0;

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

	mark_jump();

	/* TODO: allow SIGINT to stop search */
	/* FIXME: start at curpos */
	for(y = gui_y() + (rev ? -1 : 1),
			l = buffer_getindex(buffers_current(), y);
			l;
			l = (rev ? l->prev : l->next),
			rev ? y-- : y++)

		if((p = usearch(l->data, search_str))){
			int x = p - (char *)l->data;
			found = 1;
			gui_move(y, x);
			gui_status(GUI_NONE, "y=%d x=%d", y, x);
			break;
		}

	if(!found){
		gui_status(GUI_ERR, "not found");
		gui_move(gui_y(), gui_x());
	}

	return !found;
}

void shiftline(char **ps, int indent)
{
	char *s = *ps;

	if(indent){
		int len = strlen(s) + 1;
		char *new = realloc(s, len + 1);

		if(!new)
			die("realloc()");

		s = new;

		memmove(new+1, new, len);
		*new = '\t';

		*ps = s;
	}else{
		switch(*s){
			case ' ':
				if(s[1] == ' ')
					memmove(s, s+2, strlen(s+1));
				break;

			case '\t':
				memmove(s, s+1, strlen(s));
				break;
		}
	}
}

void shift(unsigned int nlines, int indent)
{
	struct list *l = buffer_getindex(buffers_current(), gui_y());

	if(!nlines)
		nlines = 1;

	while(nlines-- && l){
		shiftline((char **)&l->data, indent);
		l = l->next;
	}

	gui_move(gui_y(), gui_x());
	buffer_modified(buffers_current()) = 1;
}

void tilde(unsigned int rep)
{
	char *data = (char *)buffer_getindex(buffers_current(), gui_y())->data;
	char *pos = data + gui_x();

	if(!rep)
		rep = 1;

	gui_move(gui_y(), gui_x() + rep);

	while(rep --> 0){
		if(islower(*pos))
			*pos = toupper(*pos);
		else
			*pos = tolower(*pos);

		/* *pos ^= (1 << 5); * flip bit 100000 = 6 */

		if(!*++pos)
			break;
	}

	buffer_modified(buffers_current()) = 1;
}

void showgirl(unsigned int page)
{
	char *word = gui_current_word();
	char *buf;
	int len;

	if(!word){
		gui_status(GUI_ERR, "invalid word");
		return;
	}

	buf = umalloc(len = strlen(word) + 16);

	if(page)
		snprintf(buf, len, "man %d %s", page, word);
	else
		snprintf(buf, len, "man %s", word);

	shellout(buf, NULL);
	free(buf);
	free(word);
}

int go_file()
{
	char *fname = gui_current_fname();

	if(fname){
		/* TODO: query - rewrite gui_readfile() and so on, all goes through one f() */
		buffers_load(fname);
		free(fname);
		return 0;
	}else{
		gui_status(GUI_ERR, "no file selected");
		return 1;
	}
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

	gui_status(GUI_NONE, "\"%s\"%s %d/%d %.2f%%",
			buffer_filename(buffers_current()),
			buffer_modified(buffers_current()) ? " [Modified]" : "",
			1 + gui_y(), i,
			100.0f * (float)(1 + gui_y()) /(float)i);
}

static int findindent(const char *s)
{
	int indent = 0;

	while(*s)
		switch(*s++){
			case '\t':
				indent += global_settings.tabstop;
				break;
			case ' ':
				indent++;
				break;
			default:
				goto lewpfin;
		}

lewpfin:
	/* assuming tab indent */
	return indent / global_settings.tabstop;
}

void readlines(int do_indent, struct gui_read_opts *opts, char ***plines, int *pi)
{
	int nlines;
	char **lines;
	int indent = 0;
	int i = 0;

	nlines = 10;
	lines = umalloc(nlines * sizeof *lines);

	if(do_indent && global_settings.autoindent){
		struct list *lprev = buffer_getindex(buffers_current(), gui_y() - 1);
		if(lprev)
			indent = findindent(lprev->data);
	}

	for(;;){
		int esc;
		int saveindent = indent;

		while(indent --> 0)
			gui_ungetch('\t');

		lines[i] = NULL;
		esc = gui_getstr(&lines[i], opts);
		if(++i >= nlines)
			lines = urealloc(lines, (nlines += 10) * sizeof *lines);

		/* trim the line if it's just spaces */
		if(saveindent){
			int tabs = 0;
			char *s;

			for(s = lines[i-1]; *s; s++)
				if(*s == '\t'){
					tabs++;
				}else{
					tabs = 0;
					break;
				}

			if(tabs && tabs == saveindent){
				*lines[i-1] = '\0';
				indent = saveindent;
			}else{
				indent = findindent(lines[i-1]);
			}
		}else if(global_settings.autoindent){
			indent = findindent(lines[i-1]);
		}

		if(global_settings.cindent){
			int len = strlen(lines[i-1]);
			switch(lines[i-1][len-1]){
				case '{':
					indent++;
					break;

				case '}':
					/* need to unindent by one */
					indent--;
					shiftline(&lines[i-1], 0 /* unindent */);
					break;
			}
		}


		/* must be last, to trim, etc, before returning */
		if(esc)
			/* ^[ */
			break;
	}

	*plines = lines;
	*pi = i;
}

static void insert(int append, int do_indent)
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

	readlines(do_indent, &opts, &lines, &i);

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

	readlines(0 /* indent */, &opts, &lines, &nl);

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
	insert(0, 1);

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

	if(!applymotion(motion, &topos, &si)){
		struct range from;

		from.start = gui_y();
		from.end   = y;

		if(from.start > from.end){
			int t = from.start;
			from.start = from.end;
			from.end = t;
		}

		if(islinemotion(motion)){
			/* delete lines between gui_y() and y, inclusive */
			f_line(&from);
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
	int c = gui_getch(GETCH_COOKED);
	int x, y, dollar = 0;

	if(c == 'd' || c == 'c'){
		/* allow dd or cc (or dc or cd... but yeah) */
		motion->motion = MOTION_WHOLE_LINE;
	}else{
		gui_ungetch(c);
		if(getmotion(motion))
			return;
	}

	if(ins){
		struct bufferpos pos;
		struct screeninfo si;

		x = gui_x();
		y = gui_y();

		si.top = gui_top();
		si.height = gui_max_y();
		pos.x = &x;
		pos.y = &y;

		if(!applymotion(motion, &pos, &si))
			dollar = 1;
	}

	motion_cmd(motion, delete_line, delete_range);

	if(ins){
		if(dollar)
			gui_mvaddch(y, x > 0 ? x - 1 : x, '$');
		gui_move(gui_y(), gui_x());
		insert(0, 0);
	}
}

#ifndef alloca
void *alloca(size_t l)
{
	return umalloc(l);
}
#endif

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

		gui_move(gui_y() + 1 - rev, gui_x());

	}else{
		struct list *l = buffer_getindex(buffers_current(), gui_y());
		const int x = gui_x() + 1 - rev;
		char *data = l->data;
		char *after = alloca(strlen(data + x) + 1);
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
	struct list *jointhese, *l, *cur = buffer_getindex(buffers_current(), gui_y());
	struct range r;
	char *alloced;
	int len = 0;

	if(gui_y() + ntimes >= (unsigned)buffer_nlines(buffers_current())){
		gui_status(GUI_ERR, "can't join %d line%s", ntimes,
				ntimes > 1 ? "s" : "");
		return;
	}

	r.start = gui_y() + 1; /* extract the next line(s) */
	r.end   = r.start + ntimes;

	jointhese = buffer_extract_range(buffers_current(), &r);

	for(l = jointhese; l; l = l->next)
		len += strlen(l->data);

	alloced = realloc(cur->data, strlen(cur->data) + len + 1);
	if(!alloced)
		die("realloc()");

	cur->data = alloced;
	for(l = jointhese; l; l = l->next)
		/* TODO: add a space between these? */
		strcat(cur->data, l->data);

	list_free(jointhese, free);

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
	}

	free(in);
}

static int is_edit_char(int c)
{
	switch(c){
		case 'o':
			if(visual_get() != VISUAL_NONE)
				break;
		case 'O':
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

void gui_run()
{
	struct motion motion;
	int buffer_changed;
	int view_changed;
	int prevcmd;
	int multiple, prevmultiple;

	buffer_changed = 0;
	view_changed = 1;
	prevcmd = 0;
	prevmultiple = multiple = 0;

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

			mark_edit();
		}

switch_switch:
		switch(c){
			/*
			 * TODO: different switch when in visual mode
			 * struct cmd
			 * {
			 *   char ch;
			 *   void (*f)();
			 *   int when_visual;
			 * };
			 */
			case ':':
			{
				char buffer[16];
				if(visual_get() != VISUAL_NONE)
					snprintf(buffer, sizeof buffer, "%d,%d",
							visual_get_start()->start + 1, /* convert to 1-based */
							visual_get_end(  )->start + 1);
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
				if(!flag && visual_get() != VISUAL_NONE){
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
					insert(0, 0);
				break;

			case 'C':
				flag = 1;
			case 'D':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				motion_cmd(&motion, delete_line, delete_range);
				if(flag)
					insert(1 /* append */, 0);
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
				c = gui_getch(GETCH_COOKED);
				if(c == 'y'){
					SET_MOTION(MOTION_WHOLE_LINE);
				}else{
					gui_ungetch(c);
					if(getmotion(&motion))
						break;
				}
				motion_cmd(&motion, yank_line, yank_range);
				SET_DOT();
				break;

			case 'A':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				gui_move_motion(&motion);
			case 'a':
				flag = 1;
			case 'i':
case_i:
				insert(flag, 0);
				buffer_changed = 1;
				SET_DOT();
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

				view_changed = !search(next, rev);
				break;
			}

			case '>':
				flag = 1;
			case '<':
				shift(multiple, flag);
				buffer_changed = 1;
				SET_DOT();
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
				break;

			case '\\':
				map();
				break;

			case 'V':
				if(visual_get() != VISUAL_LINE){
					visual_set(VISUAL_LINE);
					visual_status();
				}else{
					visual_set(VISUAL_NONE);
				}
				view_changed = 1;
				break;

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

			case '!':
			{
				int ch = gui_getch(GETCH_COOKED);
				switch(ch){
					case 'f':
						if(!go_file())
							buffer_changed = 1;
						break;
					default:
						gui_status(GUI_ERR, "Invalid ! suffix");
						break;
				}
			}

			default:
				if(isdigit(c) && (c == '0' ? multiple : 1)){
					INC_MULTIPLE();
				}else{
					gui_ungetch(c);
					motion.ntimes = multiple;
					if(!getmotion(&motion)){
						if(isbigmotion(&motion) && motion.motion != MOTION_MARK)
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
