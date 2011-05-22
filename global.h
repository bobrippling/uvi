#ifndef GLOBAL_H
#define GLOBAL_H

struct settings
{
	int tabstop;
	int showtabs;
	int autoindent;
	int colour;
};

extern struct settings global_settings;

extern buffer_t *global_buffer;
extern int global_top, global_y, global_x;
extern int global_max_x, global_max_y;

#endif
