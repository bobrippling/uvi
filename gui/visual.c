#include <stdarg.h>
#include <string.h>

#include "../range.h"
#include "gui.h"
#include "visual.h"

static enum  visual visual_state = VISUAL_NONE;

static struct range visual_cursor;
static struct range visual_anchor;

void visual_update(struct range *v)
{
	v->start = gui_y();
	v->end   = gui_x();
}

void visual_set(enum visual v)
{
	if((visual_state = v) != VISUAL_NONE){
		visual_update(&visual_cursor);
		visual_update(&visual_anchor);
	}
}

enum visual visual_get()
{
	return visual_state;
}

const struct range *visual_get_start()
{
	int diff;

	visual_update(&visual_cursor);

	diff = visual_cursor.start - visual_anchor.start;

	if(diff > 0)
		return &visual_anchor;
	else if(diff < 0)
		return &visual_cursor;

	return visual_cursor.end > visual_anchor.end ? &visual_anchor : &visual_cursor;
}

const struct range *visual_get_end()
{
	if(visual_get_start() == &visual_cursor)
		return &visual_anchor;
	return &visual_cursor;
}

int visual_cursor_is_end()
{
	return visual_get_end() == &visual_cursor;
}

void visual_swap()
{
	struct range *move_to;
	struct range tmp;

	tmp           = visual_cursor;
	visual_cursor = visual_anchor;
	visual_anchor = tmp;

	if(gui_y() == visual_cursor.start && gui_x() == visual_cursor.end)
		move_to = &visual_anchor;
	else
		move_to = &visual_cursor;

	gui_move(move_to->start, move_to->end);
}

void visual_status()
{
#define VIS_INF(v) v->start + 1, v->end + 1
	const struct range *v1, *v2;
	int diff_lines;

	v1 = visual_get_start();
	v2 = visual_get_end();

	diff_lines = v2->start - v1->start + 1;

	gui_status(GUI_NONE, "(%d,%d)-(%d,%d): %d line%s",
			VIS_INF(v1),
			VIS_INF(v2),
			diff_lines,
			diff_lines == 1 ? "" : "s");
}
