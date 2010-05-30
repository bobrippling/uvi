#ifndef SET_H
#define SET_H

char set_set(const char *, const char);
char *set_get(char *);

void set_init(void);
void set_term(void);

#define SET_READONLY "ro"
#define SET_MODIFIED "modified"
#define SET_EOF      "eof"

#endif
