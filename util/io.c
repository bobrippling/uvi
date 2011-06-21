#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <termios.h>
#include <ncurses.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "alloc.h"
#include "../range.h"
#include "../buffer.h"
#include "io.h"
#include "../main.h"
#include "../gui/motion.h"
#include "../gui/intellisense.h"
#include "../gui/gui.h"
#include "../util/list.h"

int canwrite(mode_t mode, uid_t uid, gid_t gid);

/*
FIXME
int fdgetline(char **s, char **buffer, int fd)
{
	char *nl = NULL;
	int ret;

	if(!*buffer)
		*buffer = umalloc(BUFFER_SIZE);
	else
		nl = strchr(*buffer, '\n');

	if(nl){
		int len;
newline:
		len = nl - *buffer;

		*s = umalloc(len + 1);
		strncpy(*s, *buffer, len);
		(*s)[len] = '\0';

		memmove(*buffer, nl + 1, strlen(nl));

		return len;
	}else{
		switch((ret = read(fd, *buffer, BUFFER_SIZE))){
			case 0:
			case -1:
				free(*buffer);
				*buffer = NULL;
				return ret;
		}
		memset(*buffer + ret, '\0', BUFFER_SIZE - ret);
		nl = strchr(*buffer, '\n');

		if(nl){
			fputs("goto newline; !!!!!!!!!!!!!!!!!!\n", stderr);
			goto newline;
		}

		*s = *buffer;
		*buffer = NULL;
		return strlen(*s);
	}
}
*/

static struct list *mmap_to_lines(char *mem, int *haseol, size_t len)
{
	struct list *head = list_new(NULL), *cur = head;
	char *last = mem + len;
	char *a, *b;

	for(a = b = mem; a < last; a++)
		if(*a == '\n'){ /* TODO: memchr() */
			char *data = umalloc(a - b + 1);

			memcpy(data, b, a - b);

			data[a - b] = '\0';

			list_append(cur, data);
			cur = list_gettail(cur);
			b = a + 1;
		}

	if(!(*haseol = a == b)){
		char *rest = umalloc(a - b + 1);
		memcpy(rest, b, a - b);
		rest[a - b] = '\0';
		list_append(cur, rest);
	}

	return head;
}

struct list *fgetlines(FILE *f, int *haseol)
{
	struct stat st;
	struct list *l;
	int fd;
	void *mem;

	fd = fileno(f);
	if(fd == -1)
		return NULL;

	if(fstat(fd, &st) == -1)
		return NULL;

	if(st.st_size == 0)
		goto fallback; /* could be stdin */

	mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if(mem == MAP_FAILED){
		if(errno == EINVAL){
			/* fallback to fread - probably stdin */
			char buffer[256];
			struct list *l;
			struct list *i;

fallback:
			i = l = list_new(NULL);

			while(fgets(buffer, sizeof buffer, f)){
				char *nl = strchr(buffer, '\n');
				if(nl)
					*nl = '\0';
				list_append(i, ustrdup(buffer));
				i = list_gettail(i);
			}

			if(ferror(f)){
				list_free(l, free);
				return NULL;
			}
			return l;
		}

		return NULL;
	}

	l = mmap_to_lines(mem, haseol, st.st_size);
	munmap(mem, st.st_size);

	return l;
}

struct list *fnamegetlines(const char *s, int *haseol)
{
	struct list *l;
	FILE *f;

	f = fopen(s, "r");
	if(!f)
		return NULL;

	l = fgetlines(f, haseol);
	fclose(f);

	return l;
}

void chomp_line()
{
	int c;
	struct termios attr;

	tcgetattr(0, &attr);

	attr.c_cc[VMIN] = sizeof(char);
	attr.c_lflag   &= ~(ICANON | ECHO);

	tcsetattr(0, TCSANOW, &attr);

	c = getchar();

	attr.c_cc[VMIN] = 0;
	attr.c_lflag   |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &attr);

	ungetch(c);
}

int canwrite(mode_t mode, uid_t uid, gid_t gid)
{
	if(mode & 02)
		return 1;

	/* can't be bothered checking all groups */
	if((mode & 020) && gid == getgid())
		return 1;

	if((mode & 0200) && uid == getuid())
		return 1;

	return 0;
}

void *readfile(const char *filename)
{
	buffer_t *b = NULL;

	if(filename){
		FILE *f;

		if(strcmp(filename, "-")){
			f = fopen(filename, "r");
		}else{
			f = stdin;
		}

		if(f){
			int nread = buffer_read(&b, f);

			if(nread == -1){
				gui_status(GUI_ERR, "\"%s\" [%s]",
						filename,
						errno ? strerror(errno) : "unknown error - binary file?");

			}else{
				/* end up here on successful read */
				struct stat st;

				if(!stat(filename, &st))
					buffer_readonly(b) = !canwrite(st.st_mode, st.st_uid, st.st_gid);
				else
					buffer_readonly(b) = 0;

				if(nread == 0)
					gui_status(GUI_NONE, "(empty file)%s", buffer_readonly(b) ? " [read only]" : "");
				else
					gui_status(GUI_NONE, "%s%s: %dC, %dL%s", filename,
							buffer_readonly(b) ? " [read only]" : "",
							buffer_nchars(b), buffer_nlines(b),
							buffer_eol(b) ? "" : " [noeol]");
			}

			if(f == stdin){
				freopen("/dev/tty", "r", stdin);
			}else{
				fclose(f);
			}
		}else{
			if(errno == ENOENT)
				goto newfile;
			gui_status(GUI_ERR, "%s: %s", filename, strerror(errno));
		}

	}else{
newfile:
		if(filename)
			gui_status(GUI_NONE, "%s: new file", filename);
		else
			gui_status(GUI_NONE, "(new file)");
	}

	if(!b)
		b = buffer_new_empty();
	if(filename && strcmp(filename, "-"))
		buffer_setfilename(b, filename);

	return b;
}
