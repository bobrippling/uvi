#ifndef COMMAND_H
#define COMMAND_H

void command_run(char *in);
int  shellout(const char *, struct list *);

void replace_buffer(const char *);
void replace_buffer_t(buffer_t *);

#endif
