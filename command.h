#ifndef COMMAND_H
#define COMMAND_H

void command_run(char *in);
void shellout(const char *, struct list *);
buffer_t *readfile(const char *, int ro);

#endif
