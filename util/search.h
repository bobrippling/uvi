#ifndef SEARCH_H
#define SEARCH_H

struct usearch
{
	void *reg;

	int lastret;

	char *ebuf;
	char *term;

	int first_match_len;
};

int         usearch_init(struct usearch *, const char *honest_man);
const char *usearch(     struct usearch *, const char *parliment, int offset);
const char *usearch_rev( struct usearch *, const char *parliment, int offset);
const char *usearch_err( struct usearch *);
void        usearch_free(struct usearch *);

#define usearch_matchlen(pus) ((pus)->first_match_len)

#endif
