include config.mk

CFLAGS  = -g -pipe -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs \
	-Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline \
	-Wredundant-decls -Wextra -pedantic -ansi
LDFLAGS = -lncurses

ifeq (${VERBOSE},1)
	Q =
else
	Q = @
endif


uvi: main.o term.o ncurses.o view.o buffer.o range.o command.o \
	util/list.o util/map.o vars.o util/alloc.o
	@echo LD $@
	$Q${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

ncurses.o:ncurses.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -I/usr/include/ncurses -c -o $@ $<

%.o:%.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -c -o $@ $<

clean:
	${Q}rm -f *.o util/*.o uvi

.PHONY: clean

buffer.o: buffer.c util/alloc.h range.h buffer.h util/list.h
command.o: command.c range.h buffer.h command.h util/list.h vars.h \
 util/alloc.h config.h
main.o: main.c term.h ncurses.h main.h config.h
ncurses.o: ncurses.c range.h buffer.h command.h util/list.h main.h view.h \
 util/alloc.h vars.h ncurses.h config.h
range.o: range.c range.h
term.o: term.c util/list.h range.h buffer.h term.h command.h vars.h \
 config.h
vars.o: vars.c util/alloc.h buffer.h vars.h
view.o: view.c util/list.h range.h buffer.h view.h util/alloc.h
alloc.o: util/alloc.c util/alloc.h
list.o: util/list.c util/list.h util/alloc.h
