#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "rc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"
#include "global.h"
#include "util/io.h"
#include "gui/map.h"
#include "buffers.h"

static const char *rc_file(void)
{
	static char fname[256];
	char *home = getenv("HOME");

	if(!home)
		return NULL;

	snprintf(fname, sizeof fname, "%s/.uvirc", home);
	return fname;
}

int rc_read()
{
	const char *fn = rc_file();
	if(!access(fn, F_OK))
		return rc_source(fn);
	return 1;
}

int rc_source(const char *fname)
{
	char buf[512];
	FILE *f;
	int ret = 0;
	int haderr = 0;
	int lineno = 1;

	if(!fname)
		return 1;

	f = fopen(fname, "r");
	if(!f){
		fprintf(stderr, "open %s: %s\n", fname, strerror(errno));
		haderr = 1;
		goto fin;
	}

	while(fgets(buf, sizeof buf, f)){
#define MAKE_NUL(c) \
			if((p = strchr(buf, c))) \
				*p = '\0';

#define PRE "%s:%d: "
#define ARGS fname, lineno


		enum vartype type;
		char *p;
		char *start = buf;
		int bool = 1;

		MAKE_NUL('\n');
		MAKE_NUL('#');

		if(!*buf)
			continue;

		if(!strncmp("map ", start, 4)){
			char c;

			if(sscanf(start + 4, "%c %*c", &c) == 1){
				map_add(c, start + 6);
			}else{
				fprintf(stderr, PRE "invalid map \"%s\"\n", ARGS, start);
				haderr = 1;
			}

		}else{
			if(!strncmp("no", start, 2)){
				start += 2;
				bool = 0;
			}

			MAKE_NUL(' ');
			if(p)
				p++;

			type = vars_gettype(start);
			if(type == VARS_UNKNOWN){
				fprintf(stderr, PRE "unknown variable \"%s\"\n", ARGS, start);
				haderr = 1;
			}else if(vars_isbool(type)){
				if(p){
					fprintf(stderr, PRE "extraneous data (%s)\n", ARGS, p);
					haderr = 1;
				}else{
					vars_set(type, buffers_current(), bool);
				}
			}else if(p){
				vars_set(type, buffers_current(), atoi(p));
			}else{
				fprintf(stderr, PRE "need value for %s\n", ARGS, start);
				haderr = 1;
			}
		}

		lineno++;
	}

	if(ferror(f))
		ret = 1;
	fclose(f);

fin:
	if(haderr){
		fputs("any key to continue...\n", stderr);
		chomp_line();
	}

	return ret;
}
