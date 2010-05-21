#include <stdio.h>
#include <string.h>

#include "list.h"
#include "buffer.h"
#include "term.h"
#include "parse.h"

#include "config.h"

#define IN_SIZE 256
#define incorrect_cmd() do{ fputs("?\n", stdout); } while(0)

static buffer_t *buffer;
static int saved = 1;

int term_main(const char *filename)
{
	struct range rng;
	char in[IN_SIZE];
	int hadeof = 0;

	if(filename){
		int nread = buffer_read(&buffer, filename);
		if(nread < 0){
			fprintf(stderr, PROG_NAME": %s: ", filename);
			perror(NULL);
			return 1;
		}else if(nread == 0)
			fputs("(empty file)\n", stderr);
		printf("%s: %dC, %dL\n", filename, buffer_nchars(buffer), buffer_nlines(buffer));
	}else{
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


		s = parserange(in, &rng);
		/* from this point on, s/in/s/g */
		if(!s){
			puts("invalid range");
			continue;
		}
		if(s > in)
			printf("range: %d,%d\n", rng.start, rng.end);

		switch(*s){
			case '\0':
				incorrect_cmd();
				break;

			case 'q':
				if(strlen(s) > 2)
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
			{
				/* TODO: range */
				if(strlen(s) == 1){
					struct list *l = buffer->lines;
					if(l->data)
						while(l){
							puts(l->data);
							l = l->next;
						}
				}else
					incorrect_cmd();
				break;
			}

			default:
				puts("?");
		}
	}while(1);
exit_loop:

	return 0;
}
