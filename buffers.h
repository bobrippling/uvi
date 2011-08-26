#ifndef BUFFERS_H
#define BUFFERS_H

struct old_buffer
{
	const char *fname;
	int last_y;
	int read;
};


void buffers_init(int, const char **, int ro);
void buffers_term();

buffer_t    *buffers_current();
struct old_buffer **buffers_array();

int          buffers_idx();
int          buffers_count();

int          buffers_next(int n);
int          buffers_goto(int n);

/*
 * load the filename (or return error)
 * adding to the buffer list, unless already present
 */

void         buffers_load(const char *);

int          buffers_unread();

#endif
