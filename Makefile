CFLAGS = -g -pipe -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs -Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline -Wredundant-decls -Wextra -pedantic -ansi

uvi: main.o term.o buffer.o list.o alloc.o range.o
	@echo LD $@
	@${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

%.o:%.c
	@echo CC $@
	@${CC} ${CFLAGS} -c -o $@ $<

alloc.o: alloc.c alloc.h
buffer.o: buffer.c alloc.h buffer.h list.h
list.o: list.c list.h alloc.h
main.o: main.c term.h config.h term.o
range.o: range.c range.h
term.o: term.c list.h buffer.h term.h range.h config.h
test.o: test.c list.h

clean:
	rm -f *.o uvi

.PHONY: clean
