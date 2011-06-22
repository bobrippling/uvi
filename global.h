#ifndef GLOBAL_H
#define GLOBAL_H

struct settings
{
	int tabstop;
	int showtabs;
	int list;
	int autoindent;
	int colour;
	int textwidth;
};

extern struct settings global_settings;

extern buffer_t *global_buffer;

extern int global_running;

#endif
