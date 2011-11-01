#ifndef VISUAL_H
#define VISUAL_H

enum visual
{
	VISUAL_NONE,
	VISUAL_LINE,
	VISUAL_BLOCK
};

void        visual_set(enum visual);
enum visual visual_get();
void        visual_swap();
void        visual_join();

const struct range *visual_get_start();
const struct range *visual_get_end();
const struct range *visual_get_col_start();
const struct range *visual_get_col_end();

void visual_status();
int visual_cursor_is_end();

#endif
