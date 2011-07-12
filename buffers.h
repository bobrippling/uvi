#ifndef BUFFERS_H
#define BUFFERS_H

extern buffer_t *current_buffer;
void buffers_init(const char **, int ro);
int buffers_next(int n);

#endif
