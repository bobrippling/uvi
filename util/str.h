#ifndef STR_H
#define STR_H

char *word_at( const char *, int);
char *fname_at(const char *, int);

char **words_begin(struct list *, const char *);

int line_isspace(const char *s);

void  str_escape(char *arg);
char *chr_expand(char *arg, char c, const char *rep);
char *str_expand(char *str, const char *grow_from, const char *grow_to);
int   str_numeric(const char *);
int   str_eqoffset(const char *w1, const char *w2, unsigned int len, unsigned int offset);
void  str_trim(char *const);
void  str_ltrim(char *const);
void  str_rtrim(char *const);

char *str_home_replace(char *);
void  str_home_replace_array(int, char **);

char *str_shell_escape(const char *s, int *pnescapes);
void  str_shell_unescape(char *s);

int qsortstrcmp(const void *a, const void *b);
void uniq(void *base, size_t *nmemb, size_t size,
		int (*compar)(const void *, const void *),
		int (*uni)(const void *, const void *, void *),
		void *uni_extra, void (*freef)(void *));

#endif
