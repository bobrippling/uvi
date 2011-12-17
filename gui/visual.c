#include <stdarg.h>
#include <string.h>

#include "../range.h"
#include "gui.h"
#include "visual.h"

static enum  visual visual_state = VISUAL_NONE;

static struct range visual_cursor;
static struct range visual_anchor;

void visual_setpoints(int x[2], int y[2])
{
	visual_cursor.start = y[0];
	visual_cursor.end   = x[0];
	visual_anchor.start = y[1];
	visual_anchor.end   = x[1];
	gui_move(visual_cursor.start, visual_cursor.end);
}

void visual_update(struct range *v)
{
	v->start = gui_y();
	v->end   = gui_x();
}

void visual_set(enum visual v)
{
	/* if we are already in visual, don't update */
	if(visual_state == VISUAL_NONE){
		visual_update(&visual_cursor);
		visual_update(&visual_anchor);
	}

	visual_state = v;
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

const struct range *visual_get_col_start()
{
	visual_update(&visual_cursor);
	return visual_cursor.end > visual_anchor.end ? &visual_anchor : &visual_cursor;
}

const struct range *visual_get_col_end()
{
	return visual_get_col_start() == &visual_anchor ? &visual_cursor : &visual_anchor;
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

void visual_join()
{
	/* move visual_cursor to visual_anchor */
	memcpy(&visual_cursor, &visual_anchor, sizeof visual_cursor);
	gui_move(visual_cursor.start, visual_cursor.end);
}

void visual_status()
{
#define VIS_INF(v) v->start + 1, v->end + 1
#define VIS_STR(x) x, x == 1 ? "" : "s"
	const struct range *v1, *v2;
	int diff_lines, diff_cols;

	v1 = visual_get_start();
	v2 = visual_get_end();
	diff_lines = v2->start - v1->start + 1;

	v1 = visual_get_col_start();
	v2 = visual_get_col_end();
	diff_cols  = v2->end - v1->end + 1;

	gui_status(GUI_NONE, "(%d,%d)-(%d,%d): %d line%s %d column%s",
			VIS_INF(v1),
			VIS_INF(v2),
			VIS_STR(diff_lines),
			VIS_STR(diff_cols));
}
