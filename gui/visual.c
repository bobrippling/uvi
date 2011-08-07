#include <stdarg.h>
#include <string.h>

#include "../range.h"
#include "gui.h"
#include "visual.h"

static enum  visual visual_state = VISUAL_NONE;

static struct range visual_a = { -1, -1 };
static struct range visual_b = { -1, -1 };

void visual_update(struct range *v)
{
	v->start = gui_y();
	v->end   = gui_x();
}

void visual_set(enum visual v)
{
	if((visual_state = v) != VISUAL_NONE){
		visual_update(&visual_a);
		visual_update(&visual_b);
	}
}

enum visual visual_get()
{
	return visual_state;
}

const struct range *visual_get_start()
{
	int diff;

	visual_update(&visual_b);

	diff = visual_a.start - visual_b.start;

	if(diff > 0)
		return &visual_b;
	else if(diff < 0)
		return &visual_a;

	return visual_a.end > visual_b.end ? &visual_b : &visual_a;
}

const struct range *visual_get_end()
{
	if(visual_get_start() == &visual_a)
		return &visual_b;
	return &visual_a;
}

void visual_swap()
{
	struct range *move_to;
	struct range tmp;

	memcpy(&tmp,      &visual_a, sizeof tmp);
	memcpy(&visual_a, &visual_b, sizeof tmp);
	memcpy(&visual_b,      &tmp, sizeof tmp);

	if(gui_y() == visual_a.start && gui_x() == visual_a.end)
		move_to = &visual_b;
	else
		move_to = &visual_a;

	gui_move(move_to->start, move_to->end);
}

void visual_status()
{
	gui_status(GUI_NONE, "%d-%d: %d",
			visual_a.start,
			visual_b.start,
			visual_b.start - visual_a.start);
}
