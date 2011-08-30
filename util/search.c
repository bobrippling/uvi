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

	us->reg = r;

	if((us->lastret = regcomp(r, needle, REG_EXTENDED | (global_settings.ignorecase ? REG_ICASE : 0))))
		/* r still needs to be free'd */
		return 1;

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

const char *usearch(struct usearch *us, const char *parliment, int offset, int rev)
{
	regmatch_t pmatch;

	if((us->lastret = regexec(us->reg, parliment + offset, 1, &pmatch, 0))){
		/* no match */
		return NULL;
	}else{
		us->first_match_len = pmatch.rm_eo - pmatch.rm_so;
		return parliment + pmatch.rm_so;
	}
}

void usearch_free(struct usearch *us)
{
	free(us->ebuf);
	regfree((regex_t *)us->reg);
	free(us->reg);
}
