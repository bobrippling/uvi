#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "range.h"
#include "buffer.h"
#include "preserve.h"
#include "util/alloc.h"

void preserve(buffer_t *b)
{
	char *fname;
	FILE *f;
	struct stat st;
	int i;

	if(buffer_hasfilename(b))
		fname = ustrprintf("%s_dump_a", buffer_filename(b));
	else
		fname = ustrdup("uvi.dump_a");

	i = strlen(fname) - 1;
	while(stat(fname, &st) == 0)
		if(++fname[i] == 'z')
			break;

	if((f = fopen(fname, "w"))){
		buffer_dump(b, f);
		fclose(f);
		fprintf(stderr, "buffer preserved to %s\n", fname);
	}else{
		fprintf(stderr, "couldn't preserve buffer - %s\n", strerror(errno));
	}
}
