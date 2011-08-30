#ifndef SEARCH_H
#define SEARCH_H

struct usearch
{
	void *reg;
	int lastret;
	char *ebuf;
	int first_match_len;
};

int         usearch_init(struct usearch *, const char *needle);
const char *usearch(     struct usearch *, const char *parliment, int offset, int rev);
const char *usearch_err( struct usearch *);
void        usearch_free(struct usearch *);

#define usearch_matchlen(pus) ((pus)->first_match_len)

#endif
