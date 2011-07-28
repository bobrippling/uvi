#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

#include "range.h"
#include "buffer.h"
#include "buffers.h"
#include "gui/gui.h"
#include "util/io.h"
#include "util/alloc.h"

static buffer_t *current_buf;

static int arg_ro = 0;

static const char **fnames;

static int current;
static int count;

const char **buffers_array()
{
	return fnames;
}

int buffers_idx()
{
	return current;
}

buffer_t *buffers_current()
{
	return current_buf;
}

void buffers_init(int argc, const char **argv, int ro)
{
	int i;

	count = argc;
	arg_ro = ro;

	fnames = umalloc((count + 1) * sizeof *fnames);

	for(i = 0; i < count; i++)
		fnames[i] = ustrdup(argv[i]);
	fnames[count] = NULL;

	current = count > 0 ? 0 : -1;

	if(buffers_next(0))
		current_buf = buffer_new_empty();
}

static buffer_t *buffers_readfile(const char *filename)
{
	buffer_t *b = NULL;

	if(filename){
		FILE *f;
		int read_stdin;

		if(strcmp(filename, "-")){
			f = fopen(filename, "r");
			read_stdin = 0;
		}else{
			f = stdin;
			read_stdin = 1;
		}

		if(f){
			int nread = buffer_read(&b, f);

			if(nread == -1){
				gui_status(GUI_ERR, "read \"%s\": %s",
						filename,
						errno ? strerror(errno) : "unknown error - binary file?");

			}else{
				if(read_stdin)
					buffer_readonly(b) = 0;
				else
					buffer_readonly(b) = access(filename, W_OK);

				if(nread == 0)
					gui_status(GUI_NONE, "%s: empty file%s", filename, buffer_readonly(b) ? " [read only]" : "");
				else
					gui_status(GUI_NONE, "%s%s: %dC, %dL%s%s",
							filename,
							buffer_readonly(b) ? " [read only]" : "",
							buffer_nchars(b),
							buffer_nlines(b),
							buffer_eol(b)  ? "" : " [noeol]",
							buffer_crlf(b) ? " [crlf]" : ""
							);
			}

			fclose(f);
			if(read_stdin)
				input_reopen();

		}else{
			if(errno == ENOENT)
				goto newfile;
			gui_status(GUI_ERR, "open \"%s\": %s", filename, strerror(errno));
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

int buffers_next(int n)
{
	if(current == -1)
		return buffers_goto(0);
	return buffers_goto(current + n);
}

int buffers_goto(int n)
{
	if(n < 0 || n >= count)
		return 1;

	current = n;
	buffer_free(current_buf);
	current_buf = buffers_readfile(fnames[current]);

	if(arg_ro)
		buffer_readonly(buffers_current()) = 1;

	gui_move(0, 0);
	return 0;
}

void buffers_load(const char *fname)
{
	const char **iter;
	int found = 0, i;

	if(fname)
		for(i = 0, iter = fnames; *iter; iter++, i++)
			if(!strcmp(*iter, fname)){
				found = 1;
				break;
			}

	if(found){
		buffers_goto(i);
	}else if(fname){
		i = count++;
		fnames        = urealloc(fnames, (count + 1) * sizeof *fnames);
		fnames[i]     = ustrdup(fname);
		fnames[count] = NULL;
		buffers_goto(i);
	}else{
		current_buf = buffers_readfile(NULL);
		current     = -1;
	}
}
