#include <stdlib.h>
#include <stdio.h>

#include "files.h"

static const char *file_generic(const char *suffix)
{
	static char fname[256];
	const char *home = getenv("HOME");

	if(!home)
		home = "";

	snprintf(fname, sizeof fname, "%s/.uvi%s", home, suffix);

	return fname;
}

const char *file_rc(void)
{
	return file_generic("rc");
}

const char *file_info(void)
{
	return file_generic("info");
}
