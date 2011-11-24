#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <wordexp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "intellisense.h"
#include "../range.h"
#include "../buffer.h"
#include "../global.h"
#include "../util/list.h"
#include "../util/str.h"
#include "../util/alloc.h"
#include "motion.h"
#include "gui.h"
#include "../buffers.h"

#ifdef __FreeBSD__
# define WORDEXP_FREE NULL
#else
# define WORDEXP_FREE free
#endif

static int longest_match(char **words)
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

static void intellisense_complete_to(char **pstr, int *psize, int *pos,
		const char *word, int longest_len, int offset_into_word, int escape)
{
	const int space_left = *psize - *pos - longest_len + offset_into_word - 1;
	int nextra = longest_len - offset_into_word;

	if(space_left <= 0){
		*psize -= space_left;
		*pstr = urealloc(*pstr, *psize);
	}

	if(escape){
		char *copy;
		char *str;
		int nescapes;

		copy = ALLOCA(nextra + 1);
		strncpy(copy, word + offset_into_word, nextra);
		copy[nextra] = '\0';

		str = str_shell_escape(copy, &nescapes);
		nextra += nescapes;
		str_insert(pstr, psize, pos, str, nextra);
		free(str);

	}else{
		str_insert(pstr, psize, pos, word + offset_into_word, nextra);
	}

	*pos  += nextra;
}

int intellisense_insert(char **pstr, int *psize, int *pos, char ch)
{
	char **words, **iter;
	int len, wlen;
	int ret;
	char *w;
	char *savepos;
	char save;

	if(ch != CTRL_AND('n'))
		return 1;

	savepos = *pstr + *pos;
	save = *savepos;
	*savepos = '\0';
	w = word_at(*pstr, *pos - 1);
	*savepos = save;
	if(!w)
		return 1;

	ret = 1;

	wlen = strlen(w);

	words = words_begin(buffer_gethead(buffers_current()), w);
	len = longest_match(words);

	if(len - wlen > 0){
		intellisense_complete_to(pstr, psize, pos, *words, len, wlen, 0);
		ret = 0;
	}else{
		int y, x;

		gui_getyx(&y, &x);
		gui_show_array(GUI_COL_MAGENTA, y+1, x, (const char **)words);
		gui_setyx(y, x);
	}

	if(!*words)
		gui_status(GUI_ERR, "no completions");

	free(w);
	for(iter = words; *iter; iter++)
		free(*iter);
	free(words);

	return ret;
}

int file_uniq(const void *a, const void *b, void *extra)
{
	return str_eqoffset(*(const char **)a, *(const char **)b, global_settings.tabctx, *(int *)extra);
}

int intellisense_file(char **pstr, int *psize, int *pos, char ch)
{
	char *match, *arg;
	wordexp_t exp;
	int arglen, offset;
	int ret = 1;

	if(!**pstr || *pos == 0)
		return 1;

	if((match = strchr(*pstr, '*'))){
		if(match > *pstr ? match[-1] != '\\' : 1)
			return 1;
	}

	if((match = strchr(*pstr, '~')) && (match > *pstr ? match[-1] != '\\' : 1)){
		*pstr = str_home_replace(*pstr);
		*pos += strlen(*pstr) - *pos;
		*psize = *pos + 1;
	}

	if(buffer_hasfilename(buffers_current())){
		*pstr = chr_expand(*pstr, '%', buffer_filename(buffers_current()));
		*pos += strlen(*pstr) - *pos;
		*psize = *pos + 1;
	}

	{
		char *pstr_clipped;

		pstr_clipped = ustrdup(*pstr);
		pstr_clipped[*pos] = '\0';

		arg = strrchr(pstr_clipped, ' ');
		if(arg){
			for(;;){
				while(arg > pstr_clipped && *arg != ' ')
					arg--;
				if(arg == pstr_clipped)
					break;
				else if(arg[-1] != '\\')
					break;
				/* arg > pstr_clipped and we're at the space in "something\ else" */
				arg--;
			}
			if(*arg == ' ')
				arg++;
		}else{
			arg = pstr_clipped;
		}

		arglen = strlen(arg) + 2;
		match = ALLOCA(arglen);

		str_shell_unescape(arg);
		snprintf(match, arglen, "%s*", arg);

		free(pstr_clipped);
	}


	if(wordexp(match, &exp, WRDE_NOCMD))
		return 1;

	offset = arglen - 2;

	switch(exp.we_wordc){
		case 0:
			break;

		case 1:
		{
			struct stat st;
			char *s;
			int len;

			/* check for no matches */
			if(!strcmp(exp.we_wordv[0], match)){
				gui_status(GUI_NONE, ":%s  [no matches]", *pstr);
				ret = 1; /* no redraw */
				break;
			}

			len = strlen(exp.we_wordv[0]);
			s = ALLOCA(len + 2);

			if(!stat(exp.we_wordv[0], &st) && S_ISDIR(st.st_mode))
				len++, sprintf(s, "%s/", exp.we_wordv[0]);
			else
				strcpy(s, exp.we_wordv[0]);

			intellisense_complete_to(pstr, psize, pos, s, len, offset, 1);

			ret = 0;
			break;
		}

		default:
		{
			int len;

			len = longest_match(exp.we_wordv);
			if(len > offset){
				intellisense_complete_to(pstr, psize, pos, exp.we_wordv[0], len, offset, 1);
				ret = 0;
			}else{
				/* here's what you could'a won */
				char *fnames, *p;
				unsigned int i;

				/* sort|uniq */
				uniq(exp.we_wordv, &exp.we_wordc, sizeof exp.we_wordv[0],
						qsortstrcmp, file_uniq, &offset, WORDEXP_FREE);

				p = fnames = ALLOCA(exp.we_wordc * (2 + global_settings.tabctx) + 3);
				/*
				 * e.g. config.\t   (-e config.mk && -e config.h)
				 * becomes
				 * config.{mk,h}
				 *         ^- up to global_settings.tabctx chars
				 */
				*p++ = '{';
				for(i = 0; i < exp.we_wordc; i++){
					int j;

					for(j = offset; j-offset < global_settings.tabctx && exp.we_wordv[i][j]; j++)
						*p++ = exp.we_wordv[i][j];
					*p++ = ',';
					*p = '\0';
				}
				p[-1] = '}';
				/* FIXME: move cursor to correct position */

				gui_status(GUI_NONE, ":%s%s", *pstr, fnames);
				ret = 1; /* no redraw */
			}
			break;
		}
	}

	wordfree(&exp);
	return ret;
}

void intellisense_init_opt(void *v, enum intellisense_opt o)
{
	struct gui_read_opts *opts = v;
	memset(opts, 0, sizeof *opts);

	if(!global_settings.intellisense)
		return;

	switch(o){
		case INTELLI_NONE:
			break;

		case INTELLI_CMD:
			opts->intellisense    = intellisense_file;
			opts->intellisense_ch = '\t';
			opts->newline         = 0;
			break;

		case INTELLI_INS:
			opts->intellisense    = intellisense_insert;
			opts->intellisense_ch = CTRL_AND('n');
			opts->newline         = 1;
			opts->textw           = global_settings.textwidth;
			break;
	}
}
