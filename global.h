#ifndef GLOBAL_H
#define GLOBAL_H

struct settings
{
	int tabstop;
	int scrolloff;
	int showtabs;
	int list;
	int hls;
	int autoindent;
	int cindent;
	int colour;
	int textwidth;
	int fsync;
	int ignorecase;
	int tabctx;
};

extern struct settings global_settings;

extern int global_running;

#endif
