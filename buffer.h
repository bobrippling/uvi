#ifndef BUFFER_H
#define BUFFER_H

typedef struct
	{
		struct list *lines;
		/* TODO: jump list to lines->gethead(), etc */
		char *fname, haseol;
	} buffer_t;

buffer_t	*buffer_new(char *);
int				buffer_read(buffer_t **, const char *);
int				buffer_write(buffer_t *);
void			buffer_free(buffer_t *);

int				buffer_nchars(buffer_t *);
int				buffer_nlines(buffer_t *);

#define buffer_lines(x) ((x)->lines)

#endif
