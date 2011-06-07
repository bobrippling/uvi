#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rc.h"
#include "range.h"
#include "buffer.h"
#include "vars.h"
#include "global.h"
#include "util/io.h"

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
	const char *fname = rc_file();
	char buf[8];
	FILE *f;
	int ret = 0;
	int haderr = 0;
	int lineno = 1;

	if(!fname)
		return 1;

	f = fopen(fname, "r");
	if(!f)
		return 1;

	while(fgets(buf, sizeof buf, f)){
#define MAKE_NUL(c) \
			if((p = strchr(buf, c))) \
				*p = '\0';

		enum vartype type;
		char *p;
		char *start = buf;
		int bool = 1;

		MAKE_NUL('\n');
		MAKE_NUL('#');

		if(!*buf)
			continue;

		if(!strncmp("no", start, 2)){
			start += 2;
			bool = 0;
		}

		MAKE_NUL(' ');
		if(p)
			p++;

#define PRE "%s:%d: "
#define ARGS fname, lineno

		type = vars_gettype(start);
		if(type == VARS_UNKNOWN){
			fprintf(stderr, PRE "unknown variable \"%s\"\n", ARGS, start);
			haderr = 1;
		}else if(vars_isbool(type)){
			if(p){
				fprintf(stderr, PRE "extraneous data (%s)\n", ARGS, p);
				haderr = 1;
			}else{
				vars_set(type, global_buffer, bool);
			}
		}else if(p){
			vars_set(type, global_buffer, atoi(p));
		}else{
			fprintf(stderr, PRE "need value for %s\n", ARGS, start);
			haderr = 1;
		}

		lineno++;
	}

	if(ferror(f))
		ret = 1;
	fclose(f);

	if(haderr){
		fputs("any key to continue...\n", stderr);
		chomp_line();
	}

	return ret;
}
