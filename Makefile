CC      = gcc
CFLAGS  = -g -pipe -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs -Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline -Wredundant-decls -Wextra -pedantic -ansi
LDFLAGS = -lncurses

Q = @

uvi: main.o term.o ncurses.o view.o buffer.o list.o alloc.o range.o command.o
	@echo LD $@
	$Q${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

ncurses.o:ncurses.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -I/usr/include/ncurses -c -o $@ $<

%.o:%.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f *.o uvi

.PHONY: clean

alloc.o: alloc.c alloc.h
buffer.o: buffer.c alloc.h range.h buffer.h list.h
command.o: command.c range.h buffer.h command.h list.h alloc.h
list.o: list.c list.h alloc.h
main.o: main.c term.h ncurses.h main.h config.h
ncurses.o: ncurses.c buffer.h range.h command.h list.h main.h view.h \
 ncurses.h config.h
range.o: range.c range.h
term.o: term.c list.h range.h buffer.h term.h command.h config.h
test.o: test.c list.h
view.o: view.c list.h buffer.h view.h alloc.h
