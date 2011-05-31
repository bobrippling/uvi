#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>

#include "alloc.h"
#include "io.h"
#include "../main.h"

#define BUFFER_SIZE 128
/* nearest 2^n, st > 80 */

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

int fgetline(char **s, FILE *in, char *haseol)
{
	size_t buffer_size = BUFFER_SIZE;
	char *c;
	const long offset = ftell(in);

	*s = umalloc(buffer_size);

	do{
		if(!fgets(*s, buffer_size, in)){
			int eno = errno;
			if(feof(in)){
				/* no chars read */
				free(*s);
				return 0;
			}

			free(*s);
			*s = NULL;
			errno = eno;
			return -1;
		}

		if((c = strchr(*s, '\n'))){
			/* in an ideal world... */
			*c = '\0';
			*haseol = 1;
			return 1 + strlen(*s);
		}else if(feof(in)){
			/* no eol... jerks */
			*haseol = 0;
			return strlen(*s);
		}else{
			/* need a bigger biffer for this line */
			/* and buffer, too */
			char *tmp;
			buffer_size *= 2;

			tmp = realloc(*s, buffer_size);
			if(!tmp){
				free(*s);
				fprintf(stderr, __FILE__":%d: realloc failed, size %lu\n", __LINE__, buffer_size);
				die("realloc()");
			}
			*s = tmp;
			if(fseek(in, offset, SEEK_SET) == -1){
				int eno = errno;
				free(*s);
				*s = NULL;
				errno = eno;
				return -1;
			}
		}
	}while(1);
}

void chomp_line()
{
	int c;
	c = getchar();
	if(c != '\n' && c != EOF)
		while((c = getchar()) != '\n' && c != EOF);
}
