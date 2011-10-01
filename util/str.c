#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <pwd.h>

#include "../range.h"
#include "list.h"
#include "str.h"
#include "alloc.h"

#define ispwchr(x) (isalpha(x) || (x) == '_')


int iswordchar(int c)
{
	return isalnum(c) || c == '_';
}

int isfnamechar(int c)
{
	return !isspace(c) && !ispunct(c); /* TODO: ignore trailing punctuation */
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

int qsortstrcmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

int word_uniq(const void *v, const void *w, void *unused)
{
	const char *a = *(const char **)v;
	const char *b = *(const char **)w;
	return !strcmp(a, b);
}

char **words_begin(struct list *l, const char *s)
{
	const int slen = strlen(s);
	size_t nwords;
	char **words;
	size_t i = 0;

	words = umalloc(sizeof(*words) * (nwords = 10));

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

	uniq(words, &i, sizeof words[0], qsortstrcmp, word_uniq, NULL, free);

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
		if(*s == '\\'){
			int ch = '\0';

#define ESCH(a, b) case a: ch = b; break
			switch(s[1]){
				ESCH('n', '\n');
				ESCH('e', '\x1b');
			}
#undef ESCH

			if(ch){
				*s = ch;
				memmove(s + 1, s + 2, strlen(s + 1));
			}
		}
}

char *chr_expand(char *arg, char c, const char *rep)
{
#if 0
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
#else
	char s[2];
	s[0] = c;
	s[1] = '\0';
	return str_expand(arg, s, rep);
#endif
}

char *str_expand(char *str, const char *grow_from, const char *grow_to)
{
	const int from_offset = strlen(grow_from);
	char *p = strstr(str, grow_from);

	while(p){
		if((p > str ? p[-1] != '\\' : 1)){
			char *sav = str;
			*p = '\0';

			p += from_offset;

			str = ustrprintf("%s%s%s", str, grow_to, p);

			p = str + (p - sav) + strlen(grow_to) - from_offset;

			p = strstr(p, grow_from);

			free(sav);
		}else{
			/* else, unescaped, continue looking */
			p = strstr(p+1, grow_from);
		}
	}

	return str;
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

void str_trim(char *const start)
{
	char *last, *s;

	for(s = start; isspace(*s); s++);

	memmove(start, s, strlen(s) + 1);

	last = NULL;
	for(s = start; *s; s++)
		if(isspace(*s))
			last = s;

	if(last)
		*last = '\0';
}

int isescapeable(char c)
{
	switch(c){
		case ' ':
		case '\t':
		case '$':
		case '`':
		case '\\':
		case '*':
			return 1;
	}
	return 0;
}


char *str_shell_escape(const char *s, int *pnescapes)
{
	const char *p;
	char *ret, *pret;
	int newlen;
	int nescapes = 0;

	newlen = strlen(s) + 1;

	for(p = s; *p; newlen++, p++)
		if(isescapeable(*p)){
				newlen++;
				nescapes++;
		}

	if(pnescapes)
		*pnescapes = nescapes;

	ret = malloc(newlen + 1);

	for(pret = ret, p = s; *p; p++, pret++){
		if(isescapeable(*p))
			*pret++ = '\\';
		*pret = *p;
	}
	*pret = '\0';

	return ret;
}

void str_shell_unescape(char *s)
{
	int len = strlen(s);
	for(; *s; s++, len--)
		if(*s == '\\')
			memmove(s, s + 1, len);
}

int str_eqoffset(const char *w1, const char *w2, unsigned int len, unsigned int offset)
{
	if(strlen(w1) <= offset)
		return 0;
	else if(strlen(w2) <= offset)
		return 0;
	return !strncmp(w1 + offset, w2 + offset, len);
}

void uniq(void *bas, size_t *pnmemb, size_t size,
		int (*compar)(const void *, const void *),
		int (*uni)(const void *, const void *, void *),
		void *uni_extra, void (*freef)(void *))
{
	size_t nmemb = *pnmemb;
	unsigned int i;
	int **base = (int **)bas;


	qsort(base, nmemb, size, compar);

	/* uniq */
	for(i = 1; i < nmemb;)
		if(uni(&base[i], &base[i - 1], uni_extra)){
			if(freef)
				freef(base[i]);

			memmove(&base[i], &base[i+1], size * (nmemb - i));

			nmemb--;
		}else{
			i++;
		}

	*pnmemb = nmemb;
}

void str_home_replace_array(int argc, char **argv)
{
	int i;
	for(i = 0; i < argc; i++)
		argv[i] = str_home_replace(argv[i]);
}

char *str_home_replace(char *s)
{
	struct passwd *pw;
	char *p;
	char **names = NULL;
	int nnames = 0;
	int i;

	for(p = strchr(s, '~'); p && *p; p++)
		if(isalpha(p[1])){
			char *iter, old;

			for(iter = ++p; *iter && ispwchr(*iter); iter++);

			old   = *iter;
			*iter = '\0';
			pw    = getpwnam(p);
			if(pw){
				names = urealloc(names, ++nnames * sizeof *names);
				names[nnames-1] = umalloc(strlen(p) + 2);
				sprintf(names[nnames-1], "~%s", p);
			}
			*iter = old;
		}

	for(i = 0; i < nnames; i++){
		pw = getpwnam(names[i] + 1);
		if(pw)
			s = str_expand(s, names[i], pw->pw_dir);
		free(names[i]);
	}
	free(names);

	if((p = strchr(s, '~')))
		if(!ispwchr(p[1])){
			/* ~ by itself */
			const char *home;
			char *with_me;
			char rep[3];

			rep[0] = '~';
			rep[1] = p[1];
			rep[2] = '\0';

			home = getenv("HOME");
			if(!home)
				home = "/"; /* getpwuid? */

			with_me = ALLOCA(strlen(home) + 2);
			sprintf(with_me, "%s%c", home, p[1]);

			s = str_expand(s, rep, with_me);
		}

	return s;
}
