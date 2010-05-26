#ifndef BUFFER_H
#define BUFFER_H

typedef struct
	{
		struct list *lines;
		/* TODO: jump list to lines->gethead(), etc */

		char *fname,
				 haseol, changed, nlines;
		/*
		 * chagned here, means have the lines changed,
		 * NOT has the buffer been saved
		 */
	} buffer_t;

buffer_t	*buffer_new(char *);
int				buffer_read(buffer_t **, const char *);
int				buffer_write(buffer_t *);
void			buffer_free(buffer_t *);

int				buffer_nchars(buffer_t *);
int				buffer_nlines(buffer_t *);

void			buffer_setfilename(buffer_t *, const char *);

/* these can't be macros, since the buffer list pointer needs to be adjusted */
void					buffer_remove_range(buffer_t *, struct range *);
struct list	*buffer_extract_range(buffer_t *, struct range *);


/* list wrappers */
#define b2l(b) ((b)->lines)

/* helpers */
#define buffer_changed(b)									((b)->changed)
#define buffer_haseol(b)									((b)->haseol)
#define buffer_filename(b)								((b)->fname ? (b)->fname : "(empty file)")
#define buffer_hasfilename(b)							(!!(b)->fname)


/* functions that change the buffer */
#define buffer_insertbefore(b, l, d)			buffer_changed(b) = 1, list_insertbefore			(	l, d )
#define buffer_insertafter(b, l, d)				buffer_changed(b) = 1, list_insertafter				(	l, d )
#define buffer_append(b, l, d)						buffer_changed(b) = 1, list_append						(	l, d )
#define buffer_insertlistbefore(b, l, m)	buffer_changed(b) = 1, list_insertlistbefore	(	l, m )
#define buffer_insertlistafter(b, l, m)		buffer_changed(b) = 1, list_insertlistafter		(	l, m )
#define buffer_appendlist(b, l)						buffer_changed(b) = 1, list_appendlist				(	b2l(b), l )

#define buffer_extract(b, l)							buffer_changed(b) = 1, list_extract						(l)
#define buffer_remove(b, l)								buffer_changed(b) = 1, list_remove						(l)


/* read only functions */
#define buffer_getindex(b, m)							list_getindex	(	b2l(b), m)
/* TODO: jump table in buffer? */
#define buffer_indexof(b, l)							list_indexof	( b2l(b), l)

/* TODO: store head and tail pointers in buffer */
#define buffer_gethead(b)									list_gethead	( b2l(b) )
#define buffer_gettail(b)									list_gettail	( b2l(b) )

#endif
