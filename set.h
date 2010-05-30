#ifndef SET_H
#define SET_H

void set_set(const char *, const char);
char *set_get(char *);

void set_init(void);
void set_term(void);

#define SET_READONLY "ro"
#define SET_MODIFIED "modified"

#endif
