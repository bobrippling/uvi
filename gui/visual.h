#ifndef VISUAL_H
#define VISUAL_H

enum visual
{
	VISUAL_NONE,
	VISUAL_LINE
};

void        visual_set(enum visual);
enum visual visual_get();
void        visual_swap();
void        visual_join();

const struct range *visual_get_start();
const struct range *visual_get_end();

void visual_status();
int visual_cursor_is_end();

#endif
