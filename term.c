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
static char *gfunc(char *, int);
static void wrongfunc(void);

static void wrongfunc()
{
	fputs("?\n", stdout);
}

static char *gfunc(char *s, int i)
{
	return fgets(s, i, stdin);
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
			puts("(empty file)\n");
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

		if(!runcommand(in, buffer,
					&curline, &saved,
					&wrongfunc, &pfunc,
					&gfunc, &qfunc))
			break;
	}while(1);

	return 0;
}
