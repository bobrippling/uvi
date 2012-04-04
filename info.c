#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "info.h"
#include "gui/marks.h"
#include "files.h"
#include "range.h"
#include "util/list.h"
#include "yank.h"
#include "util/io.h"
#include "util/alloc.h"
#include "global.h"

#define info_open(m) fopen(file_info(), m)

void info_read(void)
{
	FILE *f;
	char *line;

	if(!global_settings.read_info)
		return;

	f = info_open("r");
	if(!f)
		return;

	while((line = fline(f, NULL))){
		switch(*line){
			case '\'':
			{
				char c;
				int y, x;
				/* mark */
				if(sscanf(line + 1, "%c %d %d\n", &c, &y, &x) == 3)
					mark_set(c, y, x);
				break;
			}

			case '"':
			{
				/* yank */
				char c;
				if(sscanf(line + 1, "%c", &c) == 1){
					char *pos = line + 2;
					if(*pos == '\0'){
						/* multiline yank */
						struct list *y = list_new(NULL);

						for(;;){
							free(line);
							line = fline(f, NULL);
							if(!line)
								break; /* ignore */

							if(*line == '\t')
								list_append(y, ustrdup(line + 1));
							else
								break;
						}

						if(list_count(y))
							yank_set_list(c, y);
						else
							list_free(y, free);

					}else{
						/* single line */
						yank_set_str(c, ustrdup(pos + 1));
					}
				}
				break;
			}
		}
		free(line);
	}

	fclose(f);
}

void info_write(void)
{
	const char *path = file_info();
	FILE *f;
	int mode;
	int i;

	mode = access(path, F_OK) == -1 && errno == ENOENT;

	f = info_open("w");
	if(!f)
		return;

	if(mode)
		chmod(path, 0600);

#if 0
- The command line history
- The search string history
- The input-line history
- Last search/substitute pattern (for 'n' and '&')
#endif

	/* marks */
	ITER_MARKS(i){
		int x, y;
		if(mark_get_idx(i, &y, &x) == 0)
			fprintf(f, "'%c %d %d\n", mark_from_idx(i), y, x);
	}

	/* registers */
	ITER_YANKS(i){
		struct yank *y = yank_get(i);
		if(y->v){
			fprintf(f, "\"%c", i);
			if(y->is_list){
				struct list *l;
				fputc('\n', f);
				for(l = y->v; l; l = l->next)
					fprintf(f, "\t%s\n", (char *)l->data);
			}else{
				fprintf(f, " %s\n", (char *)y->v);
			}
		}
	}

	fclose(f);
}
