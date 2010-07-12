#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "alloc.h"
#include "io.h"

int fgetline(char **s, FILE *in, char *haseol)
{
#define BUFFER_SIZE 128
/* nearest 2^n, st > 80 */
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
				fprintf(stderr, __FILE__":%d: realloc failed, size %ld\n", __LINE__, buffer_size);
				/* %zd corresponds to size_t */
				longjmp(allocerr, ALLOC_BUFFER_C);
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
#undef BUFFER_SIZE
}
