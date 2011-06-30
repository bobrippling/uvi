#ifndef BUFFER_H
#define BUFFER_H

typedef struct
{
	struct list *lines;
	/* TODO: jump list to lines->gethead(), etc */

	char *fname;
	int readonly;
	int modified;
	int eol;
	int crlf;

	/* internal variables */
	int dirty;
	int nlines;
	time_t opentime;
} buffer_t;

buffer_t *buffer_new(char *);
buffer_t *buffer_new_empty(void);
buffer_t *buffer_new_list(struct list *l);

int buffer_read(buffer_t **, FILE *f);
int buffer_write(buffer_t *);
int buffer_external_modified(buffer_t *);

void buffer_free(buffer_t *);
void buffer_free_nolist(buffer_t *); /* free(), leaving the list in memory */

void buffer_replace(buffer_t *, struct list *);

int buffer_nchars(buffer_t *);
int buffer_nlines(buffer_t *);

void buffer_setfilename(buffer_t *, const char *);

/* these can't be macros, since the buffer list pointer needs to be adjusted */
void buffer_remove_range(buffer_t *, struct range *);
struct list *buffer_extract_range(buffer_t *, struct range *);

void buffer_dump(buffer_t *, FILE *);

/* list wrappers */
#define b2l(b) ((b)->lines)

/* helpers */
#define buffer_modified(b)                ((b)->modified)
#define buffer_readonly(b)                ((b)->readonly)
#define buffer_eol(b)                     ((b)->eol)
#define buffer_crlf(b)                    ((b)->crlf)
#define buffer_opentime(b)                ((b)->opentime)

#define buffer_filename(b)                ((b)->fname ? (b)->fname : "(no name)")
#define buffer_hasfilename(b)             (!!(b)->fname)


/* functions that change the buffer */
#define buffer_insertbefore(b, l, d)      ( (b)->dirty = 1, list_insertbefore      ( l, d )         )
#define buffer_insertafter(b, l, d)       ( (b)->dirty = 1, list_insertafter       ( l, d )         )
#define buffer_append(b, l, d)            ( (b)->dirty = 1, list_append            ( l, d )         )
#define buffer_insertlistbefore(b, l, m)  ( (b)->dirty = 1, list_insertlistbefore  ( l, m )         )
#define buffer_insertlistafter(b, l, m)   ( (b)->dirty = 1, list_insertlistafter   ( l, m )         )
#define buffer_appendlist(b, l)           ( (b)->dirty = 1, list_appendlist        ( b2l(b), l )    )

#define buffer_extract(b, l)              ( (b)->dirty = 1, list_extract           (l)              )
#define buffer_remove(b, l)               ( (b)->dirty = 1, list_remove            (l)              )

#define buffer_copy_range(b, r)           list_copy_range(b2l(b), (void *(*)(void *))ustrdup, r)

/* read only functions */
#define buffer_getindex(b, m)             list_getindex ( b2l(b), m)
/* TODO: jump table in buffer? */
#define buffer_indexof(b, l)              list_indexof  ( b2l(b), l)

/* TODO: store head and tail pointers in buffer */
#define buffer_gethead(b)                 list_gethead  ( b2l(b) )
#define buffer_gettail(b)                 list_gettail  ( b2l(b) )

#endif
