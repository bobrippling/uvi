#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <wordexp.h>

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
				return i;
	}

	return minlen;
}

void intellisense_complete_to(char **pstr, int *psize, int *pos,
		const char *word, int longest_len, int offset_into_word)
{
	const int space_left = *psize - *pos - longest_len + offset_into_word - 1;
	const int nextra = longest_len - offset_into_word;

	if(space_left <= 0){
		*psize -= space_left;
		*pstr = urealloc(*pstr, *psize);
	}

	strncat(*pstr + *pos, word + offset_into_word, nextra);

	*pos += nextra;
}

int intellisense_insert(char **pstr, int *psize, int *pos, char ch)
{
	char **words, **iter;
	int len, wlen;
	int ret;
	char *w;

	if(ch != CTRL_AND('n'))
		return 1;

	w = word_at(*pstr, *pos - 1);
	if(!w)
		return 1;

	ret = 1;

	wlen = strlen(w);

	words = words_begin(buffer_gethead(buffers_current()), w);
	len = longest_match(words);

	if(len - wlen > 0){
		intellisense_complete_to(pstr, psize, pos, *words, len, wlen);
		ret = 0;
	}else{
		int y, x;

		gui_getyx(&y, &x);
		gui_show_array(GUI_COL_WHITE, y+1, x, (const char **)words);
		gui_setyx(y, x);
	}

	free(w);
	for(iter = words; *iter; iter++)
		free(*iter);
	free(words);

	return ret;
}

int qsortstrcmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

int intellisense_file(char **pstr, int *psize, int *pos, char ch)
{
	char *match, *arg;
	wordexp_t exp;
	int arglen, offset;
	int ret = 1;

	if(strchr(*pstr, '*'))
		return 1;

	if((match = strchr(*pstr, '~'))){
		const char *home;
		int len;

		/* FIXME: ~root, etc */
		if(!(home = getenv("HOME")))
			home = "/";

		*pstr = str_expand(*pstr, '~', home);
		*pos += strlen(home) - 1;
		if(*psize < (len = strlen(*pstr)))
			*psize += len + 64;
		/* reallocate up to original size + a bit */
		*pstr = urealloc(*pstr, *psize);
		return 0;
	}

	arg = strrchr(*pstr, ' ');
	if(arg){
		for(;;){
			while(arg > *pstr && *arg != ' ')
				arg--;
			if(arg == *pstr)
				break;
			else if(arg[-1] != '\\')
				break;
			/* arg > pstr and we're at the space in "something\ else" */
			arg--;
		}
		if(*arg == ' ')
			arg++;
	}else{
		arg = *pstr;
	}

	arglen = strlen(arg) + 2;
	match = alloca(arglen);

	snprintf(match, arglen, "%s*", arg);

	if(wordexp(match, &exp, WRDE_NOCMD))
		return 1;

	offset = arglen - 2;

	switch(exp.we_wordc){
		case 0:
			break;

		case 1:
			intellisense_complete_to(pstr, psize, pos,
					exp.we_wordv[0], strlen(exp.we_wordv[0]), offset);
			ret = 0;
			break;

		default:
		{
			int len;

			len = longest_match(exp.we_wordv);
			if(len > offset){
				intellisense_complete_to(pstr, psize, pos, exp.we_wordv[0], len, offset);
				ret = 0;
			}else{
#define SUFFIX_LEN 5
				/* here's what you could'a won */
				char *fnames, *p;
				unsigned int i;

				/* sort */
				qsort(exp.we_wordv, exp.we_wordc, sizeof exp.we_wordv[0], qsortstrcmp);
				/* uniq */
				for(i = 1; i < exp.we_wordc;)
					if(str_eqoffset(exp.we_wordv[i], exp.we_wordv[i-1], SUFFIX_LEN, offset)){
						unsigned int j;
						free(exp.we_wordv[i]);
						for(j = i + 1; j <= exp.we_wordc; j++)
							exp.we_wordv[j - 1] = exp.we_wordv[j];
						exp.we_wordc--;
					}else{
						i++;
					}

				p = fnames = alloca(exp.we_wordc * (2 + SUFFIX_LEN) + 3);
				/*
				 * e.g. config.\t   (-e config.mk && -e config.h)
				 * becomes
				 * config.{mk,h}
				 *         ^- up to SUFFIX_LEN chars
				 */
				*p++ = '{';
				for(i = 0; i < exp.we_wordc; i++){
					int j;

					for(j = offset; j-offset < SUFFIX_LEN && exp.we_wordv[i][j]; j++)
						*p++ = exp.we_wordv[i][j];
					*p++ = ',';
					*p = '\0';
				}
				p[-1] = '}';
				gui_status(GUI_NONE, ":%s%s", *pstr, fnames);
				ret = 1; /* no redraw */
			}
			break;
		}
	}

	wordfree(&exp);
	return ret;
}
