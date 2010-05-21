#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "list.h"
#include "buffer.h"
#include "term.h"
#include "parse.h"

#include "config.h"

#define IN_SIZE 256
#define incorrect_cmd() do{ fputs("?\n", stdout); } while(0)

static buffer_t *buffer;
static int saved = 1;

int validate_range(char *, char *, struct range *);

int validate_range(char *s, char *in, struct range *rng)
{
	if(s > in){
		/* validate range */
		int lines = list_count(buffer->lines);

		if(rng->start == RANGE_FIRST)
			rng->start = 1;
		if(rng->end == RANGE_FIRST)
			rng->end = 1;
		if(rng->start == RANGE_LAST)
			rng->start = lines;
		if(rng->end == RANGE_LAST)
			rng->end = lines;

		if(rng->start < 1 || rng->start > lines ||
			 rng->end   < 1 || rng->end   > lines){
			puts("out of range");
			return 0;
		}else if(rng->start > rng->end){
			int ans;
			fputs("swap backward range? (Y/n) ", stdout);
			fflush(stdout);
			ans = getchar();

			if(ans == EOF){
				putchar('\n');
				return 0;
			}else if(ans != '\n')
				/* swallow the rest of the line */
				do
					switch(getchar()){
						case EOF:
						case '\n':
							goto getchar_break;
					}
				while(1);
getchar_break:


			if(ans == '\n' || tolower(ans) == 'y'){
				ans = rng->start;
				rng->start = rng->end;
				rng->end   = ans;
			}else
				return 0;
		}
	}

	return 1;
}


int term_main(const char *filename)
{
	struct range rng;
	char in[IN_SIZE];
	int hadeof = 0, curline = 1;

	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			if(errno == ENOENT)
				goto new_file;

			fprintf(stderr, PROG_NAME": %s: ", filename);
			perror(NULL);
			return 1;
		}else if(nread == 0)
			fputs("(empty file)\n", stderr);
		else
			printf("%s: %dC, %dL%s\n", filename, buffer_nchars(buffer),
					buffer_nlines(buffer), buffer->haseol ? "" : " (noeol)");
	}else{
new_file:
		buffer = buffer_new();
		puts("(new file)");
	}

	do{
		char *s;

		if(!fgets(in, IN_SIZE, stdin)){
			if(hadeof)
				goto exit_loop;
			incorrect_cmd();
			hadeof = 1;
			continue;
		}

		hadeof = 0;
		s = strchr(in, '\n');
		if(s)
			*s = '\0';

		if(!strcmp(in, "DUMP")){
			struct list *l = buffer->lines;
			int i = 0;
			puts("--- DUMP START ---");
			while(l){
				printf("%d: \"%s\"\n", ++i, l->data);
				l = l->next;
			}
			puts("--- DUMP END ---");
		}

		s = parserange(in, &rng);
		/* from this point on, s/in/s/g */
		if(!s){
			puts("invalid range");
			continue;
		}
		if(!validate_range(s, in, &rng))
			continue;


		switch(*s){
			case '\0':
				incorrect_cmd();
				break;

			case 'q':
				if(s > in || strlen(s) > 2)
					incorrect_cmd();
				else{
					switch(s[1]){
						case '\0':
							if(!saved){
								puts("unsaved");
								break;
							}else
						case '!':
								goto exit_loop;

						default:
								incorrect_cmd();
					}
				}
				break;

			case 'p':
				if(strlen(s) == 1){
					struct list *l;
					if(s > in){
						int i = rng.start - 1;

						for(l = list_getindex(buffer->lines, i);
								i++ != rng.end;
								l = l->next)
								puts(l->data);

					}else{
				    l = buffer->lines;
						if(l->data)
							while(l){
								puts(l->data);
								l = l->next;
							}
					}
				}else
					incorrect_cmd();
				break;

			case 'd':
				if(strlen(s) == 1){
					struct list *l;
					if(s > in){
						l = list_getindex(buffer->lines, rng.start - 1);

						list_remove_range(&l, rng.end - rng.start);

						buffer->lines = list_gethead(l);
					}else
						list_remove(list_getindex(buffer->lines, curline - 1));
				}else
					incorrect_cmd();
				break;

			default:
				puts("?");
		}
	}while(1);
exit_loop:

	return 0;
}
