#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../range.h"
#include "list.h"
#include "str.h"
#include "alloc.h"

#define iswordchar(c) (isalnum(c) || c == '_')

char *word_at(const char *line, int x)
{
	const char *wordstart, *wordend;
	char *word;
	int len;

	wordend = wordstart = line + x;

	if(!iswordchar(*wordstart))
		return NULL;

	while(wordstart > line && iswordchar(wordstart[-1]))
		wordstart--;

	while(*wordend && iswordchar(*wordend))
		wordend++;

	len = wordend - wordstart + 1;

	word = umalloc(len);

	strncpy(word, wordstart, len);
	word[len - 1] = '\0';

	return word;
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
