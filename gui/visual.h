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

const struct range *visual_get_start();
const struct range *visual_get_end();

#endif
