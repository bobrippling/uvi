#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

#include "../range.h"
#include "../buffer.h"
#include "../command.h"
#include "../util/list.h"
#include "../global.h"
#include "motion.h"
#include "../util/alloc.h"
#include "gui.h"
#include "marks.h"
#include "../main.h"

#define iswordchar(c) (isalnum(c) || c == '_')
#define REPEAT_FUNC(nam) static void nam(unsigned int)

/* text altering */
static void open(int); /* as in, 'o' & 'O' */
static void shift(unsigned int, int);
static void delete(struct motion *);
static void insert(int insert /* bool */);
REPEAT_FUNC(join);
REPEAT_FUNC(tilde);
REPEAT_FUNC(replace);

/* extra */
static int  colon(void);
static int  search(int);
REPEAT_FUNC(showgirl);

static char iseditchar(int);

static void showpos(void);


#define SEARCH_STR_SIZE 256
static char searchstr[SEARCH_STR_SIZE] = { 0 };


static int search(int next)
{
	char *pos;
	int done = 0, i = 0;
	struct list *l = buffer_gethead(global_buffer); /* FIXME: start at curpos */


	if(next){
		if(*searchstr){
			done = 1; /* exit immediately */
			goto dosearch;
		}else{
			gui_status("no previous search");
			return 0;
		}
	}

	gui_mvaddch(gui_max_y(), 0, '/');
	while(!done){
		if((searchstr[i] = gui_getch()) == EOF)
			done = 1; /* TODO: return to initial position */
		else{
			struct list *s;
			int y;

			/* FIXME: check for backspace */
			gui_mvaddch(gui_max_y(), strlen(searchstr), searchstr[i++]);

			/* TODO: allow SIGINT to stop search */
			if((pos = strchr(searchstr, '\n'))){
				*pos = '\0';
				done = 1;
			}

dosearch:
			for(y = 0, s = l; s; s = s->next){
				if((pos = strstr(l->data, searchstr))){
					int x = pos - (char *)s->data;
					gui_move(x, y);
					break;
				}

				y++;
			}
		}
	}

	return 0;
}

void shift(unsigned int nlines, int indent)
{
	struct list *l = buffer_getindex(global_buffer, gui_y());

	if(!nlines)
		nlines = 1;

	while(nlines-- && l){
		char *data = l->data;

		if(indent){
			int len = strlen(data) + 1;
			char *new = realloc(data, len + 1);

			if(!new)
				die("realloc()");

			l->data = new;

			memmove(new+1, new, len);
			*new = '\t';
		}else
			switch(*data){
				case ' ':
					if(data[1] == ' ')
						memmove(data, data+2, strlen(data+1));
					break;

				case '\t':
					memmove(data, data+1, strlen(data));
					break;
			}

		l = l->next;
	}

	buffer_modified(global_buffer) = 1;
}

void tilde(unsigned int rep)
{
	char *data = (char *)buffer_getindex(global_buffer, gui_x())->data;
	char *pos = data + gui_x();

	while(rep--){
		if(islower(*pos))
			*pos = toupper(*pos);
		else
			*pos = tolower(*pos);

		/* *pos ^= (1 << 5); * flip bit 100000 = 6 */

		if(!*++pos)
			break;
	}

	gui_move(gui_x() + pos - data, gui_y());

	buffer_modified(global_buffer) = 1;
}

void showgirl(unsigned int page)
{
	char *const line = buffer_getindex(global_buffer, gui_y())->data;
	char *wordstart, *wordend, *word;
	char save;
	int len;

	wordend = wordstart = line + gui_x();

	if(!iswordchar(*wordstart)){
		gui_status("invalid word");
		return;
	}

	while(--wordstart >= line && iswordchar(*wordstart));
	wordstart++;

	while(iswordchar(*wordend))
		wordend++;

	len = wordend - wordstart;

	word = umalloc(len += 8);
	len--;
	strncpy(word, wordstart, len);
	word[len] = '\0'; /* TODO test */

	save = wordend[1];
	wordend[1] = '\0';
	if(page)
		snprintf(word, len, "man %d %s", page, wordstart);
	else
		snprintf(word, len, "man %s", wordstart);
	wordend[1] = save;

	shellout(word, NULL);
	free(word);
}

void replace(unsigned int n)
{
	int c;
	struct list *cur = buffer_getindex(global_buffer, gui_y());
	char *s = cur->data;

	if(!*s)
		return;

	if(n <= 0)
		n = 1;
	else if(n > strlen(s + gui_x()))
		return;

	c = gui_getch();

	if(c == '\n'){
		/* delete n chars, and insert 1 line */
		char *off = s + gui_x() + n-1;
		char *cpy = umalloc(strlen(off));

		memset(off - n + 1, '\0', n);
		strcpy(cpy, off + 1);

		buffer_insertafter(global_buffer, cur, cpy);

		gui_move(0, gui_y() + 1);
	}else{
		int x = gui_x();

		while(n--)
			s[x + n] = c;
	}

	buffer_modified(global_buffer) = 1;
}

static void showpos()
{
	const int i = buffer_nlines(global_buffer);

	gui_status("\"%s\"%s %d/%d %.2f%%",
			buffer_filename(global_buffer),
			buffer_modified(global_buffer) ? " [Modified]" : "",
			1 + gui_y(), i,
			100.0f * (float)(1 + gui_y()) /(float)i);
}

static void insert(int append)
{
	char *buf = umalloc(256);
	if(append)
		gui_move(gui_y(), gui_x() + 1);

	if(gui_getstr(buf, 256)){
		free(buf);
		return;
	}

	buffer_insertafter(global_buffer, buffer_getindex(global_buffer, gui_y()), buf);
	gui_move(gui_y(), gui_x() + strlen(buf));

	buffer_modified(global_buffer) = 1;
}

static void open(int before)
{
	struct list *here;

	if(before)
		gui_move(gui_y() - 1, gui_x());

	here = buffer_getindex(global_buffer, gui_y());

	list_insertafter(here, ustrdup(""));
	insert(0);
}

static void delete(struct motion *mparam)
{
	struct bufferpos topos;
	struct screeninfo si;
	int x = gui_x(), y = gui_y();

	topos.x = &x;
	topos.y = &y;
	si.top = gui_top();
	si.height = gui_max_y();

	if(applymotion(mparam, &topos, &si)){
		struct range r;

		r.start = gui_y();
		r.end   = y;

		if(r.start > r.end){
			int t = r.start;
			r.start = r.end;
			r.end = t;
		}


		if(islinemotion(mparam)){
			/* delete lines between gui_y() and y, inclusive */
			buffer_remove_range(global_buffer, &r);
			gui_clip();
		}else{
			char *data = buffer_getindex(global_buffer, gui_y())->data, forwardmotion = 1;
			int oldlen = strlen(data);

			if(r.start < r.end){
				/* lines to remove */
				r.start++;
				buffer_remove_range(global_buffer, &r);

				/* join line r.end with line y */
				join(1);
				x += oldlen;

				data = buffer_getindex(global_buffer, gui_y())->data;
			}

			/* should be left-most */
			if(gui_x() > x){
				int t = x;
				x = gui_x();
				gui_move(gui_y(), t);
				forwardmotion = 0;
			}

			{
				char *linestart = data;
				char *curpos    = linestart + gui_x();
				char *xpos      = linestart + x;

				if(forwardmotion){
					if(*xpos != '\0' && !istilmotion(mparam))
						xpos++; /* delete up to and including where we motion() to */
				}else{
					if(*xpos != '\0'){
						curpos++;
						/* include the current char in the deletion */
						xpos++;
					}
				}

				/* remove the chars between gui_x() and x, inclusive */
				memmove(curpos, xpos, strlen(xpos) + 1);

				if(gui_x() >= x)
					gui_move(gui_y(), x);
			}
		}
	}

	buffer_modified(global_buffer) = 1;
}

static void join(unsigned int ntimes)
{
	struct list *jointhese, *l, *cur = buffer_getindex(global_buffer, gui_y());
	struct range r;
	char *alloced;
	int len = 0;

	if(ntimes == 0)
		ntimes = 1;

	if(gui_y() + ntimes >= (unsigned)buffer_nlines(global_buffer)){
		gui_status("can't join %d line%s", ntimes,
				ntimes > 1 ? "s" : "");
		return;
	}

	r.start = gui_y() + 1; /* extract the next line(s) */
	r.end   = r.start + ntimes;

	jointhese = buffer_extract_range(global_buffer, &r);

	for(l = jointhese; l; l = l->next)
		len += strlen(l->data);

	alloced = realloc(cur->data, strlen(cur->data) + len + 1);
	if(!alloced)
		die("realloc()");

	cur->data = alloced;
	for(l = jointhese; l; l = l->next)
		/* TODO: add a space between these? */
		strcat(cur->data, l->data);

	list_free(jointhese);

	buffer_modified(global_buffer) = 1;
}

static int colon()
{
	char in[128];

	if(!gui_prompt(":", in, sizeof in)){
		char *c = strchr(in, '\n');

		if(c)
			*c = '\0';

		command_run(in);
	}

	return 1;
}

static char iseditchar(int c)
{
	switch(c){
		case 'O':
		case 'o':
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
			return 1;
	}
	return 0;
}

int gui_main(const char *filename, char readonly)
{
	int c, bufferchanged = 1, viewchanged = 1, ret = 0, multiple = 0;
	int prevcmd = '\0', prevmultiple = 0;
	struct motion motion;

	if(!isatty(0))
		fputs("uvi: warning: input is not a terminal\n", stderr);
	if(!isatty(1))
		fputs("uvi: warning: output is not a terminal\n", stderr);

	gui_init();
	global_buffer = readfile(filename, readonly);


	do{
		int flag = 0, resetmultiple = 1;

		if(bufferchanged){
			bufferchanged = 0;
			viewchanged = 1;
		}
		if(viewchanged){
			/*
			 * cursor must be updated before
			 * the pad is refreshed
			 */
			gui_status("at (%d, %d)", gui_x(), gui_y());
			gui_redraw();
			viewchanged = 0;
		}

#define INC_MULTIPLE() \
				do \
					if(multiple < INT_MAX/10){ \
						multiple = multiple * 10 + c - '0'; \
						resetmultiple = 0; \
					}else \
						gui_status("range too large"); \
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
		c = gui_getch();
		if(iseditchar(c) && buffer_readonly(global_buffer)){
			gui_status("global_buffer is read-only");
			continue;
		}

		switch(c){
			case ':':
				colon();
				/* need to view_refresh_or_whatever() */
				bufferchanged = 1;
				break;

			case '.':
				if(prevcmd){
					ungetch(prevcmd);
					multiple = prevmultiple;
					goto switch_start;
				}else
					gui_status("no previous command");
				break;

			case 'm':
				c = gui_getch();
				if(mark_valid(c))
					mark_set(c, gui_y(), gui_x());
				else
					gui_status("invalid mark");
				break;

			case CTRL_AND('g'):
				showpos();
				viewchanged = 1;
				break;

			case 'O':
				flag = 1;
			case 'o':
				open(flag);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'X':
			case 'x':
				SET_MOTION(c == 'X' ? MOTION_BACKWARD_LETTER : MOTION_FORWARD_LETTER);
				delete(&motion);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'C':
				flag = 1;
			case 'D':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				delete(&motion);
				if(flag)
					insert(1 /* append */);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'S':
				ungetch('c');
			case 'c':
				flag = 1;
			case 'd':
				c = gui_getch();
				if(c == 'd' || c == 'c'){
					/* allow dd or cc (or dc or cd... but yeah) */
					SET_MOTION(MOTION_NOMOVE);
				}else{
					ungetch(c);
					if(getmotion(&motion))
						break;
				}
				delete(&motion);
				viewchanged = 1;
				if(flag)
					insert(0);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'A':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				gui_move_motion(&motion);
			case 'a':
				flag = 1;
			case 'i':
case_i:
				insert(flag);
				bufferchanged = 1;
				SET_DOT();
				break;
			case 'I':
				SET_MOTION(MOTION_LINE_START);
				gui_move_motion(&motion);
				goto case_i;

			case 'J':
				join(multiple);
				bufferchanged = 1;
				SET_DOT();
				break;

			case 'r':
				replace(multiple);
				bufferchanged = 1;
				SET_DOT();
				break;

			case CTRL_AND('f'):
				viewchanged = gui_scroll(PAGE_DOWN);
				break;
			case CTRL_AND('b'):
				viewchanged = gui_scroll(PAGE_UP);
				break;
			case CTRL_AND('d'):
				viewchanged = gui_scroll(HALF_DOWN);
				break;
			case CTRL_AND('u'):
				viewchanged = gui_scroll(HALF_UP);
				break;
			case CTRL_AND('e'):
				viewchanged = gui_scroll(SINGLE_DOWN);
				break;
			case CTRL_AND('y'):
				viewchanged = gui_scroll(SINGLE_UP);
				break;

			case 'n':
				flag = 1;
			case '/':
				viewchanged = search(flag);
				break;

			case '>':
				flag = 1;
			case '<':
				shift(multiple, flag);
				bufferchanged = 1;
				SET_DOT();
				break;

			case '~':
				tilde(multiple ? multiple : 1);
				SET_DOT();
				break;

			case 'K':
				showgirl(multiple);
				break;

			case 'z':
				/* screen move - vim's zz, zt & zb */
				switch(gui_getch()){
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
				break;

			case CTRL_AND('['):
				break;
			case CTRL_AND('l'):
				gui_redraw();
				break;

			default:
				if(isdigit(c) && (c == '0' ? multiple : 1))
					INC_MULTIPLE();
				else{
					ungetch(c);
					motion.ntimes = multiple;
					if(!getmotion(&motion)){
						gui_move_motion(&motion);
						viewchanged = 1;
					}
				}
		}

		if(resetmultiple)
			multiple = 0;
	}while(global_running);

	gui_term();
	buffer_free(global_buffer);

	/*
	 * apparently this uses uninitialised memory
	 * signal(SIGINT,  oldinth);
	 * signal(SIGSEGV, oldsegh);
	 */

	return ret;
#undef INC_MULTIPLE
#undef SET_DOT
}
