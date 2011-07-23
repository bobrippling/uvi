#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../range.h"
#include "list.h"
#include "str.h"
#include "alloc.h"

int iswordchar(int c)
{
	return isalnum(c) || c == '_';
}

int isfnamechar(int c)
{
	return !isspace(c); /* TODO: ignore trailing punctuation */
}

char *chars_at(const char *line, int x, int (*cmp)(int))
{
	const char *wordstart, *wordend;
	char *word;
	int len;

	wordend = wordstart = line + x;

	if(!cmp(*wordstart))
		return NULL;

	while(wordstart > line && cmp(wordstart[-1]))
		wordstart--;

	while(*wordend && cmp(*wordend))
		wordend++;

	len = wordend - wordstart + 1;

	word = umalloc(len);

	strncpy(word, wordstart, len);
	word[len - 1] = '\0';

	return word;
}

char *word_at(const char *line, int x)
{
	return chars_at(line, x, iswordchar);
}

char *fname_at(const char *line, int x)
{
	return chars_at(line, x, isfnamechar);
}

char **words_begin(struct list *l, const char *s)
{
	const int slen = strlen(s);
	int nwords;
	char **words = umalloc(sizeof(*words) * (nwords = 10));
	int i = 0;

	for(; l; l = l->next){
		char *p;

		if((p = strstr(l->data, s))){
			char *w = word_at(l->data, p - (char *)l->data);

			if(strncmp(w, s, slen) == 0){
				words[i++] = w;
				if(i == nwords)
					words = urealloc(words, sizeof(*words) * (nwords += 10));
			}
		}
	}
	words[i] = NULL;

	return words;
}

int line_isspace(const char *s)
{
	for(;;)
		switch(*s++){
			default:
				return 0;
			case '\0':
				return 1;
			case '\t':
			case ' ':
				break;
		}
}

void str_escape(char *arg)
{
	char *s;

	for(s = arg; *s; s++)
		if(*s == '\\' && s[1] == 'n'){
			*s = '\n';
			memmove(s+1, s+2, strlen(s+1));
		}
}

char *str_expand(char *arg, char c, const char *rep)
{
	char *p = strchr(arg, c);

	while(p){
		if((p > arg ? p[-1] != '\\' : 1)){
			char *sav = arg;
			*p++ = '\0';

			arg = ustrprintf("%s%s%s", arg, rep, p);

			p = strchr(arg + (p - sav) + strlen(rep) - 1, c);

			free(sav);
		}else{
			/* else, unescaped, continue looking */
			p = strchr(p+1, c);
		}
	}

	return arg;
}

int str_numeric(const char *s)
{
	if(!*s)
		return 0;

	while(*s){
		if('0' <= *s && *s <= '9'){
			s++;
			continue;
		}
		return 0;
	}
	return 1;
}


#define SEARCH_SIMPLE
char *usearch(const char *parliment, const char *honest_man)
{
#ifdef SEARCH_SIMPLE
	return strstr(parliment, honest_man);
#else
	/* TODO: regex */
#endif
}
