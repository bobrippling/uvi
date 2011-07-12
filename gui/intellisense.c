#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "intellisense.h"
#include "../range.h"
#include "../buffer.h"
#include "../global.h"
#include "../util/str.h"
#include "../util/list.h"
#include "../util/alloc.h"
#include "motion.h"
#include "gui.h"
#include "../buffers.h"

int longest_match(char **words)
{
	int minlen = INT_MAX;
	int i;
	char **iter;

	if(!*words)
		return 0;

	for(iter = words; *iter; iter++){
		const int len = strlen(*iter);

		if(len < minlen)
			minlen = len;
	}


	for(i = 0; i < minlen; i++){
		char c = (*words)[i];
		char **jter;

		for(jter = words + 1; *jter; jter++)
			if((*jter)[i] != c)
				return i - 1;
	}

	/* no difference */
	return minlen;
}

int intellisense_insert(char **pstr, int *psize, int *pos, char ch)
{
	char **words, **iter;
	int len, wlen, diff;
	char *w;

	if(*pos == 0)
		return 0;

	(*pstr)[*pos] = '\0';
	w = word_at(*pstr, *pos - 1);
	if(!w)
		return 0;

	wlen = strlen(w);

	words = words_begin(buffer_gethead(current_buffer), w);
	len = longest_match(words);

	if((diff = len - wlen) > 0){
		int space_left = *psize - *pos - diff;

		if(space_left <= 0){
			*psize -= space_left;
			*pstr = urealloc(*pstr, *psize);
		}

		strncat(*pstr + *pos, *words + wlen, len - wlen + 1);
		*pos += len - wlen;
	}else{
		int n;

		for(n = 0, iter = words; *iter; iter++, n++);

		gui_status(GUI_ERR, "can't complete - %d words", n);
	}

	free(w);
	for(iter = words; *iter; iter++)
		free(*iter);
	free(words);

	return 0;
}

