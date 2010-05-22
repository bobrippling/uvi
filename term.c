#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#include "list.h"
#include "buffer.h"
#include "term.h"
#include "range.h"
#include "command.h"

#include "config.h"

#define IN_SIZE 256

static buffer_t *buffer;
static int saved = 1;

static int	qfunc(const char *);
static void pfunc(const char *, ...);
static void wrongfunc(void);

static void wrongfunc()
{
	fputs("?\n", stdout);
}

static int qfunc(const char *s)
{
	int ans;
	fputs(s, stdout);

	ans = getchar();

	if(ans == EOF){
		putchar('\n');
		return 0;
	}else if(ans != '\n')
		/* swallow the rest of the line */
		do
			switch(getchar()){
				case EOF:
				case '\n':
					goto getchar_break;
			}
		while(1);
getchar_break:

	return ans == '\n' || tolower(ans) == 'y';
}

static void pfunc(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	vprintf(s, l);
	va_end(l);
	putchar('\n');
}

int term_main(const char *filename)
{
	struct range rng;
	char in[IN_SIZE];
	int hadeof = 0, curline = 1;

	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			if(errno == ENOENT)
				goto new_file;

			fprintf(stderr, PROG_NAME": %s: ", filename);
			perror(NULL);
			return 1;
		}else if(nread == 0)
			fputs("(empty file)\n", stderr);
		else
			printf("%s: %dC, %dL%s\n", filename, buffer_nchars(buffer),
					buffer_nlines(buffer), buffer->haseol ? "" : " (noeol)");
	}else{
new_file:
		buffer = buffer_new();
		puts("(new file)");
	}

	do{
		char *s;
		struct range lim;

		if(!fgets(in, IN_SIZE, stdin)){
			if(hadeof){
				if(saved || hadeof >= 2)
					return 0;
				else
					puts("not saved");
			}else
				wrongfunc();
			hadeof++;
			continue;
		}

		hadeof = 0;
		s = strchr(in, '\n');
		if(s)
			*s = '\0';

		if(!strcmp(in, "DUMP")){
			struct list *l = buffer->lines;
			int i = 0;
			puts("--- DUMP START ---");
			while(l){
				printf("%d: \"%s\"\n", ++i, l->data);
				l = l->next;
			}
			puts("--- DUMP END ---");
		}

		lim.start = curline;
		lim.end	 = list_count(buffer->lines);

		s = parserange(in, &rng, &lim, &qfunc, &pfunc);
		/* from this point on, s/in/s/g */
		if(!s)
			continue;
		else if(s > in && strlen(s) == 0){
			/* just a number, move to that line */
			curline = rng.start - 1;
			continue;
		}

		if(!runcommand(s, s > in ? &rng : NULL, buffer,
					list_getindex(buffer->lines, curline),
					&saved, wrongfunc, pfunc))
			break;
	}while(1);

	return 0;
}
