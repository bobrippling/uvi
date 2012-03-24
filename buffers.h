#ifndef BUFFERS_H
#define BUFFERS_H

struct old_buffer
{
	char *fname;
	int last_y;
	int read;
};


void buffers_init(int, const char **, int ro);
void buffers_term(void);

buffer_t    *buffers_current(void);
struct old_buffer **buffers_array(void);

int          buffers_idx(void);
int          buffers_count(void);

int          buffers_next(int n);
int          buffers_goto(int n);
int          buffers_del( int n);

/*
 * load the filename (or return error)
 * adding to the buffer list, unless already present
 */

int          buffers_add(     const char *);
void         buffers_load(    const char *);
int          buffers_at_fname(const char *);

int          buffers_unread(void);
int          buffers_first_unread(void);

const char  *buffers_alternate(void);
int          buffers_alternate_idx(void);

#endif
