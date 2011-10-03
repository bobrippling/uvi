#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "gui.h"
#include "../range.h"
#include "../buffer.h"
#include "../buffers.h"
#include "../util/list.h"
#include "../util/alloc.h"
#include "extra.h"
#include "../command.h"
#include "motion.h"
#include "../util/pipe.h"

int go_file()
{
	char *fname = gui_current_fname();

	if(fname){
		buffers_load(fname);
		free(fname);
		return 0;
	}else{
		gui_status(GUI_ERR, "no file selected");
		return 1;
	}
}

int fmt_motion()
{
	struct range rng;
	int x[2];

	if(motion_wrap(&x[0], &rng.start, &x[1], &rng.end, "", 0))
		return 1;

	buffer_modified(buffers_current()) = 1;
	return range_through_pipe(&rng, "fmt -80");
}

void tilde(unsigned int rep)
{
	char *data = (char *)buffer_getindex(buffers_current(), gui_y())->data;
	char *pos = data + gui_x();

	if(!rep)
		rep = 1;

	gui_move(gui_y(), gui_x() + rep);

	while(rep --> 0){
		if(islower(*pos))
			*pos = toupper(*pos);
		else
			*pos = tolower(*pos);

		/* *pos ^= (1 << 5); * flip bit 100000 = 6 */

		if(!*++pos)
			break;
	}

	buffer_modified(buffers_current()) = 1;
}

void showgirl(unsigned int page)
{
	char *word = gui_current_word();
	char *buf;
	int len;

	if(!word){
		gui_status(GUI_ERR, "invalid word");
		return;
	}

	buf = umalloc(len = strlen(word) + 16);

	if(page)
		snprintf(buf, len, "man %d %s", page, word);
	else
		snprintf(buf, len, "man %s", word);

	shellout(buf, NULL);
	free(buf);
	free(word);
}
