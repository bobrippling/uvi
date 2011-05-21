#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#if 0
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#include "../main.h"
#include "../vars.h"
#include "marks.h"
#endif

#include "../range.h"
#include "../buffer.h"
#include "../command.h"
#include "../util/list.h"
#include "../global.h"
#include "motion.h"
#include "../util/alloc.h"

#define CTRL_AND(c)  ((c) & 037)
#define iswordchar(c) (isalnum(c) || c == '_')
#define REPEAT_FUNC(nam) static void nam(unsigned int)

static void sigh(int);

/* text altering */
static int  open(int); /* as in, 'o' & 'O' */
static void shift(unsigned int, int);
static void delete(struct motion *);
static void insert(int insert /* bool */);
REPEAT_FUNC(join);
REPEAT_FUNC(tilde);
REPEAT_FUNC(replace);
static void shellout(const char *cmd);

/* extra */
static int  colon(void);
static int  search(int);
REPEAT_FUNC(showgirl);

static void unknownchar(int);
static char iseditchar(int);

static void showpos(void);


static buffer_t *buffer;
#define SEARCH_STR_SIZE 256
static char searchstr[SEARCH_STR_SIZE] = { 0 };


static int search(int next)
{
	char *pos;
	int done = 0, i = 0;
	struct list *l = buffer_gethead(buffer); /* FIXME: start at curpos */


	if(next){
		if(*searchstr){
			done = 1; /* exit immediately */
			goto dosearch;
		}else{
			gui_status("no previous search");
			return 0;
		}
	}

	gui_addch(gui_maxy(), 0, '/');
	while(!done){
		if(gui_readchar(searchstr, SEARCH_STR_SIZE, i))
			done = 1; /* TODO: return to initial position */
		else{
			struct list *s;
			int y;

			/* FIXME: check for backspace */
			gui_addchar(gui_maxy(), strlen(searchstr), searchstr[i++]);

			/* TODO: allow SIGINT to stop search */
			if((pos = strchr(searchstr, '\n'))){
				*pos = '\0';
				done = 1;
			}

dosearch:
			for(y = 0, s = l; s; s = s->next){
				if((pos = strstr(l->data, searchstr))){
					int x = pos - (char *)s->data;
					gui_cursor(x, y);
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
	struct list *l = buffer_getindex(buffer, global_y);

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

	buffer_modified(buffer) = 1;
}

void tilde(unsigned int rep)
{
	char *data = (char *)buffer_getindex(buffer, gui_x())->data;
	char *pos = data + global_x;
	const int len = strlen(data);

	while(rep--){
		if(islower(*pos))
			*pos = toupper(*pos);
		else
			*pos = tolower(*pos);

		/* *pos ^= (1 << 5); * flip bit 100000 = 6 */

		if(!*++pos)
			break;
	}

	gui_cursor(gui_x() + pos - data, gui_y());

	buffer_modified(buffer) = 1;
}

void showgirl(unsigned int page)
{
	char *const line = buffer_getindex(buffer, global_y)->data;
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

	shellout(word);
	free(word);
}

void replace(unsigned int n)
{
	int c;
	struct list *cur = buffer_getindex(buffer, gui_y());
	char *s = cur->data;

	if(!*s)
		return;

	if(n <= 0)
		n = 1;
	else if(n > strlen(s + gui_x()))
		return;

	c = gui_getchar();

	if(c == '\n'){
		/* delete n chars, and insert 1 line */
		char *off = s + gui_x() + n-1;
		char *cpy = umalloc(strlen(off));

		memset(off - n + 1, '\0', n);
		strcpy(cpy, off + 1);

		buffer_insertafter(buffer, cur, cpy);

		gui_cursor(0, gui_y() + 1);
	}else{
		int x = gui_x();

		while(n--)
			s[x + n] = c;
	}

	buffer_modified(buffer) = 1;
}

static void showpos()
{
	const int i = buffer_nlines(buffer);

	gui_status("\"%s\"%s %d/%d %.2f%%",
			buffer_filename(buffer),
			buffer_modified(buffer) ? " [Modified]" : "", 1 + gui_x(),
			i, 100.0f * (float)(1 + gui_y()) /(float)i);
}

int qfunc(const char *s, ...)
{
	va_list l;
	int c;

	va_start(l, s);
	gui_vstatus(s, l);
	va_end(l);

	c = gui_getchar();
	return c == '\n' || tolower(c) == 'y';
}

static void shellout(const char *cmd)
{
	int ret;

	gui_down();
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

	gui_up();
}

static void wrongfunc(void)
{
	gui_status("?");
}

static void unknownchar(int c)
{
	gui_status("unrecognised character #%d", c);
}


static void insert(int append)
{
	struct list *listpos = buffer_getindex(buffer, gui_x());
	struct list *new;
	char *after, *generic_str_ptr;

	if(append)
		gui_setx(gui_x() + 1);

	generic_str_ptr = after = listpos->data + gui_x();
	after = ustrdup(after);
	*generic_str_ptr = '\0';

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
								die("realloc()");
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


	/* snip */
	firstline->next = NULL;
	if(new)
		new->prev = NULL;

	listpos->data = ustrcat(&listpos->data, new->data);

	if(new)
		list_insertlistafter(listpos, new);

	buffer_modified(buffer) = 1;

	gui_setx(gui_x() + strlen(after));
}

static void open(int before)
{
	struct list *here;

	if(before)
		gui_sety(gui_y() - 1);

	here = buffer_getindex(buffer, gui_y());

	list_insertafter(here, ustrdup(""));
	insert(0);
}

static void delete(struct motion *mparam)
{
	struct bufferpos topos;
	struct screeninfo si;
	int x = padx, y = pady;

	topos.buffer = buffer;
	topos.x = &x;
	topos.y = &y;
	si.padtop = padtop;
	si.padheight = MAX_Y;

	if(applymotion(mparam, &topos, &si)){
		struct range r;

		if(pady > y){
			int t = y;
			y = pady;
			pady = t;
		}

		r.start = pady;
		r.end   = y;

		if(islinemotion(mparam)){
			/* delete lines between pady and y, inclusive */
			buffer_remove_range(buffer, &r);
			if(pady >= buffer_nlines(buffer))
				pady = buffer_nlines(buffer) - 1;
		}else{
			char *data = buffer_getindex(buffer, pady)->data, forwardmotion = 1;
			int oldlen = strlen(data);

			if(r.start < r.end){
				/* lines to remove */
				r.start++;
				buffer_remove_range(buffer, &r);

				/* join line pady with line y */
				join(1);
				x += oldlen;

				data = buffer_getindex(buffer, pady)->data;
			}

			/* padx should be left-most */
			if(padx > x){
				int t = x;
				x = padx;
				padx = t;
				forwardmotion = 0;
			}

			{
				char *linestart = data;
				char *curpos    = linestart + padx;
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

				/* remove the chars between padx and x, inclusive */
				memmove(curpos, xpos, strlen(xpos) + 1);

				if(padx >= x)
					padx = x;
			}
		}
	}

	buffer_modified(buffer) = 1;
}

static void join(int ntimes)
{
	struct list *jointhese, *l, *cur = buffer_getindex(buffer, pady);
	struct range r;
	char *alloced;
	int len = 0;

	if(ntimes == 0)
		ntimes = 1;

	if(pady + ntimes >= buffer_nlines(buffer)){
		gui_status("can't join %d line%s", ntimes,
				ntimes > 1 ? "s" : "");
		return;
	}

	r.start = pady + 1; /* extract the next line(s) */
	r.end   = r.start + ntimes;

	jointhese = buffer_extract_range(buffer, &r);

	for(l = jointhese; l; l = l->next)
		len += strlen(l->data);

	alloced = realloc(cur->data, strlen(cur->data) + len + 1);
	if(!alloced)
		longjmp(allocerr, 1);

	cur->data = alloced;
	for(l = jointhese; l; l = l->next)
		/* TODO: add a space between these? */
		strcat(cur->data, l->data);

	list_free(jointhese);

	buffer_modified(buffer) = 1;
}

static int colon()
{
#define BUF_SIZE 128
	char in[BUF_SIZE], *c;
	int ret;

	(void)mvaddch(MAX_Y, 0, ':');
	gfunc_onpad = 0;

	switch(gfunc(in, BUF_SIZE)){
		case g_CONTINUE:
			c = strchr(in, '\n');

			if(c)
				*c = '\0';

			ret = command_run(in,
					&pady, &buffer, &wrongfunc, &pfunc,
					&gfunc, &qfunc, &shellout, &nc_toggle);

			return ret;

		case g_LAST: /* esc means cancel in this sense */
		case g_EOF:
			break;
	}

	return 1;
#undef BUF_SIZE
}

static void sigh(int sig)
{
	nc_down();
	bail(sig);
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
	int c, bufferchanged = 1, viewchanged = 1, ret = 0, multiple = 0,
			prevcmd = '\0', prevmultiple = 0;
	void (*oldinth)(int), (*oldsegh)(int);
	struct motion motion;

	if(!isatty(STDIN_FILENO))
		fputs(PROG_NAME": warning: input is not a terminal\n", stderr);
	if(!isatty(STDOUT_FILENO))
		fputs(PROG_NAME": warning: output is not a terminal\n", stderr);

	nc_up();

	if(!(buffer = command_readfile(filename, readonly, pfunc))){
		/* TODO: leave ncurses up, with empty buffer */
		nc_down();
		fprintf(stderr, PROG_NAME": %s: ", filename);
		perror(NULL);
		ret = 1;
		goto fin;
	}

	/* have to be after buffer's been initialised */
	if(!debug){
		oldinth = signal(SIGINT, &sigh);
		oldsegh = signal(SIGSEGV, &sigh);
	}

	do{
		int flag = 0, resetmultiple = 1;

#if NCURSES_DEBUG_SHOWXY
		gui_status("at (%d, %d): %c", padx, pady, ((char *)buffer_getindex(buffer, pady)->data)[padx]);
		view_updatecursor();
#endif
		if(pfunc_wantconfimation){
			gui_status("---");
			pfunc_wantconfimation = 0;
			gui_status("press any key...");
			ungetch(nc_getch());
		}
		if(bufferchanged){
			bufferchanged = 0;
			viewchanged = 1;
		}
		if(viewchanged){
			/*
			 * cursor must be updated before
			 * the pad is refreshed
			 */
			view_cursoronscreen();
			viewchanged = 0;
		}
		/*
		 * view.c handles cursor positioning
		 * either way, cursor placement should be _before_
		 * view_refreshpad
		 */

#define INC_MULTIPLE() \
				do \
					if(multiple < INT_MAX/10){ \
						multiple = multiple * 10 + c - '0'; \
						resetmultiple = 0; \
					}else \
						pfunc("range too large"); \
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
		c = nc_getch();
		if(iseditchar(c) && buffer_readonly(buffer)){
			gui_status(READ_ONLY_ERR);
			view_cursoronscreen();
			continue;
		}

		switch(c){
			case ':':
				if(!colon())
					goto exit_while;

				/* need to view_refresh_or_whatever() */
				bufferchanged = 1;
				break;

			case '.':
				if(prevcmd){
					ungetch(prevcmd);
					multiple = prevmultiple;
					goto switch_start;
				}else
					pfunc("no previous command");
				break;

			case 'm':
				c = nc_getch();
				if(validmark(c))
					mark_set(c, pady, padx);
				else
					gui_status("invalid mark");
				break;

			case CTRL_AND_g:
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
				SET_MOTION(MOTION_BACKWARD_LETTER); /* TODO: test */
				delete(&motion);
				bufferchanged = 1;
				SET_DOT();
				break;
			case 'x':
				SET_MOTION(MOTION_NO_MOVE);
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
				c = nc_getch();
				if(c == 'd' || c == 'c'){
					/* allow dd or cc (or dc or cd... but yeah) */
					SET_MOTION(MOTION_LINE);
				}else{
					ungetch(c);
					getmotion(&status, &nc_getch, &motion);
				}
				if(motion.motion != MOTION_UNKNOWN){
					delete(&motion);
					viewchanged = 1;
					if(flag)
						insert(0);
					bufferchanged = 1;
					SET_DOT();
				}
				break;

			case 'A':
				SET_MOTION(MOTION_ABSOLUTE_RIGHT);
				view_move(&motion);
			case 'a':
				flag = 1;
			case 'i':
case_i:
				insert(flag);
				bufferchanged = 1;
				SET_DOT();
				break;
			case 'I':
				SET_MOTION(MOTION_NOSPACE);
				view_move(&motion);
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
				viewchanged = view_scroll(PAGE_DOWN);
				break;
			case CTRL_AND('b'):
				viewchanged = view_scroll(PAGE_UP);
				break;
			case CTRL_AND('d'):
				viewchanged = view_scroll(HALF_DOWN);
				break;
			case CTRL_AND('u'):
				viewchanged = view_scroll(HALF_UP);
				break;
			case CTRL_AND('e'):
				viewchanged = view_scroll(SINGLE_DOWN);
				break;
			case CTRL_AND('y'):
				viewchanged = view_scroll(SINGLE_UP);
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
				switch(nc_getch()){
					case 'z':
						view_scroll(CURSOR_MIDDLE);
						break;
					case 't':
						view_scroll(CURSOR_TOP);
						break;
					case 'b':
						view_scroll(CURSOR_BOTTOM);
						break;
				}
				break;

			case C_ESC:
				break;

			default:
				if(isdigit(c) && (c == '0' ? multiple : 1))
					INC_MULTIPLE();
				else{
					ungetch(c);
					motion.ntimes = multiple;
					getmotion(&status, &nc_getch, &motion);

					if(motion.motion != MOTION_UNKNOWN)
						view_move(&motion);
				}
		}

		if(resetmultiple)
			multiple = 0;
	}while(1);

exit_while:

	nc_down();
fin:
	buffer_free(buffer);
	command_free();

	/*
	 * apparently this uses uninitialised memory
	 * signal(SIGINT,  oldinth);
	 * signal(SIGSEGV, oldsegh);
	 */

	return ret;
#undef INC_MULTIPLE
#undef SET_DOT
}
