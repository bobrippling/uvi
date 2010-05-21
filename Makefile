CFLAGS = -g -pipe -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs -Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline -Wredundant-decls -Wextra -pedantic -ansi

uvi: main.o term.o buffer.o list.o alloc.o parse.o
	@echo LD $@
	@${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

%.o:%.c
	@echo CC $@
	@${CC} ${CFLAGS} -c -o $@ $<

main.o: main.c term.o
term.o: term.c term.h buffer.h buffer.o parse.h parse.o
parse.o: parse.c parse.h
buffer.o: buffer.c buffer.h list.h list.o
list.o: list.c list.h alloc.h alloc.o
alloc.o: alloc.c alloc.h

clean:
	rm -f *.o uvi

.PHONY: clean
