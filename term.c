#include <stdio.h>
#include <string.h>

#include "list.h"
#include "buffer.h"
#include "term.h"

#include "config.h"

#define IN_SIZE 256
#define incorrect_cmd() do{ fputs("?\n", stdout); } while(0)

static buffer_t *buffer;
static int saved = 1;

int term_main(const char *filename)
{
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
		char *c;

		if(!fgets(in, IN_SIZE, stdin)){
			if(hadeof)
				goto exit_loop;
			incorrect_cmd();
			hadeof = 1;
			continue;
		}

		hadeof = 0;
		c = strchr(in, '\n');
		if(c)
			*c = '\0';


		/* TODO: parse range */
		switch(*in){
			case '\0':
				incorrect_cmd();
				break;

			case 'q':
				if(strlen(in) > 2)
					incorrect_cmd();
				else{
					switch(in[1]){
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
				if(strlen(in) == 1){
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
