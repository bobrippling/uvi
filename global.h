#ifndef GLOBAL_H
#define GLOBAL_H

struct settings
{
	int tabstop;
	int showtabs;
	int list;
	int scrolloff;
	int textwidth;
	int tabctx;

	int ignorecase;

	int hls;
	int syn;

	int autoindent;
	int cindent;
	int func_motion_vi;
	int et;

	int fsync;
	int wtrim;
};

extern struct settings global_settings;

extern int global_running;

#endif
