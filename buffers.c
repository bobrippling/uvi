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

static struct old_buffer **fnames;

static int current;
static int count;

struct old_buffer **buffers_array()
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

struct old_buffer *new_old_buf(const char *fname)
{
	struct old_buffer *b;
	b = umalloc(sizeof *b);
	memset(b, 0, sizeof *b);
	b->fname  = ustrdup(fname);
	return b;
}

static buffer_t *buffers_readfile(FILE *f, const char *filename)
{
	buffer_t *b;
	int nread;

	nread = buffer_read(&b, f);

	if(nread == -1){
		gui_status(GUI_ERR, "read \"%s\": %s",
				filename,
				errno ? strerror(errno) : "unknown error - binary file?");

	}else{
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

	return b;
}

static buffer_t *buffers_readfname(const char *filename)
{
	buffer_t *b = NULL;

	if(filename){
		FILE *f;

		f = fopen(filename, "r");

		if(f){
			b = buffers_readfile(f, filename);
			fclose(f);
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
	if(filename)
		buffer_setfilename(b, filename);

	return b;
}

void buffers_init(int argc, const char **argv, int ro)
{
	int i;
	int read_stdin = 0;

	if(argc > 0 && !strcmp(argv[0], "-")){
		argc--;
		argv++;
		read_stdin = 1;
	}

	count  = argc;
	arg_ro = ro;

	fnames = umalloc((count + 1) * sizeof *fnames);

	for(i = 0; i < count; i++)
		fnames[i] = new_old_buf(argv[i]);

	fnames[count] = NULL;

	current = count > 0 ? 0 : -1;

	if(read_stdin){
		current_buf = buffers_readfile(stdin, "-");
		input_reopen();
		current = -1;
	}else if(count > 0){
		current_buf = buffers_readfname(*argv);
	}else{
		current_buf = buffer_new_empty();
	}
}

int buffers_next(int n)
{
	if(current == -1)
		return buffers_goto(0);
	return buffers_goto(current + n);
}

void buffers_save_pos()
{
	if(0 <= current && current < count)
		/* save position */
		fnames[current]->last_y = gui_y();
}

int buffers_goto(int n)
{
	if(n < 0 || n >= count)
		return 1;

	buffers_save_pos();

	current = n;
	buffer_free(current_buf);
	current_buf = buffers_readfname(fnames[current]->fname);

	if(arg_ro)
		buffer_readonly(buffers_current()) = 1;

	gui_move(fnames[n]->last_y, 0);
	gui_scroll(CURSOR_MIDDLE);
	return 0;
}

void buffers_load(const char *fname)
{
	struct old_buffer **iter;
	int found = 0, i;

	if(fname)
		for(i = 0, iter = fnames; *iter; iter++, i++)
			if(!strcmp((*iter)->fname, fname)){
				found = 1;
				break;
			}

	buffers_save_pos();

	if(found){
		buffers_goto(i);
	}else if(fname){
		i = count++;
		fnames        = urealloc(fnames, (count + 1) * sizeof *fnames);

		fnames[i]     = new_old_buf(fname);

		fnames[count] = NULL;
		buffers_goto(i);
	}else{
		current_buf = buffers_readfname(NULL);
		current     = -1;
		gui_move(0, 0);
	}
}
