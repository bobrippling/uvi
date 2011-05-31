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

	if(!fname)
		return 1;

	f = fopen(fname, "r");
	if(!f)
		return 1;

	while(fgets(buf, sizeof buf, f)){
#define MAKE_NUL(c) \
			if((p = strchr(buf, c))) \
				*p = '\0';

#define ERR(...) \
		do{ \
			fprintf(stderr, __VA_ARGS__); \
			haderr = 1; \
		}while(0)

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

		type = vars_gettype(start);
		if(type == VARS_UNKNOWN){
			ERR("%s: unknown variable \"%s\"\n", fname, start);
		}else if(vars_isbool(type)){
			vars_set(type, global_buffer, bool);
		}else if(p){
			vars_set(type, global_buffer, atoi(p));
		}else{
			ERR("%s: need value for %s\n", fname, start);
		}
	}

	if(ferror(f))
		ret = 1;
	fclose(f);

	if(haderr){
		fputs("enter to continue...", stderr);
		chomp_line();
	}

	return ret;
}
