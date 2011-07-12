#include <stdio.h>
#include <time.h>

#include "range.h"
#include "buffer.h"
#include "buffers.h"
#include "gui/gui.h"

buffer_t *current_buffer;

static int arg_ro = 0;

static const char **fnames;
static int start;
static int current;
static int end;

void buffers_init(const char **argv, int ro)
{
	for(end = 0; end[argv]; end++);

	fnames = argv;
	arg_ro = ro;
	buffers_next(0);
}

int buffers_next(int n)
{
	int i = current + n;

	if(i < start || i >= end)
		return 1;

	current = i;
	current_buffer = gui_readfile(fnames[current]);

	if(fnames[current]){
		if(arg_ro)
			buffer_readonly(current_buffer) = 1;
		return 0;
	}
	return 1;
}
