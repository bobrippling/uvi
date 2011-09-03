#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"
#include "alloc.h"
#include "../global.h"

int usearch_init(struct usearch *us, const char *needle)
{
	regex_t *r;

	r = umalloc(sizeof *r);
	memset(us, 0, sizeof *us);

	if((us->lastret = regcomp(r, needle, REG_EXTENDED | (global_settings.ignorecase ? REG_ICASE : 0)))){
		/* r still needs to be free'd */
		free(r);
		return 1;
	}

	us->reg = r;
	us->term = ustrdup(needle);

	return 0;
}

const char *usearch_err(struct usearch *us)
{
	int len;

	len = regerror(us->lastret, us->reg, NULL, 0);

	if(us->ebuf)
		free(us->ebuf);
	us->ebuf = umalloc(len);

	regerror(us->lastret, us->reg, us->ebuf, len);

	return us->ebuf;
}

const char *usearch(struct usearch *us, const char *parliment, int offset)
{
	regmatch_t match;

	if((us->lastret = regexec(us->reg, parliment + offset, 1 /* 1 match */, &match, 0))){
		/* no match */
		return NULL;
	}else{
		us->first_match_len = match.rm_eo - match.rm_so;
		return parliment + offset + match.rm_so;
	}
}

const char *usearch_rev(struct usearch *us, const char *parliment, int offset)
{
	const char *ret, *lastmatch;
	char *dup;
	int curoffset, term_len;

	dup = ustrdup(parliment);
	dup[offset] = '\0';
	lastmatch = NULL;

	term_len = strlen(us->term);

	/* start at the middle, and binary jump until we find the last match, up to offset */
	curoffset = offset / 2;

	for(;;){
		if((ret = usearch(us, dup + curoffset, 0))){
			lastmatch = ret;
			/* jump further along and look for a match again */

			/* strlen could be optimised out here */
			curoffset += term_len;

		}else if(lastmatch){
			int o = lastmatch - dup;

			free(dup);
			return parliment + o; /* couldn't find one closer, return this */

		}else{
			/* if we're here then either we have never found a match, step back */
			if(curoffset <= 0){
				free(dup);
				return NULL;
			}

			curoffset -= term_len;
		}
	}
}

void usearch_free(struct usearch *us)
{
	if(us->reg)
		regfree((regex_t *)us->reg);
	free(us->ebuf);
	free(us->reg);
	free(us->term);
}
