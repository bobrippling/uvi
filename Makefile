include config.mk

VERBOSE ?= 0

ifeq (${VERBOSE},1)
	Q =
else
	Q = @
endif

OBJS = main.o term.o buffer.o range.o command.o vars.o \
	util/list.o util/alloc.o \
	ncurses/view.o ncurses/ncurses.o ncurses/motion.o \
	ncurses/marks.o


uvi: ${OBJS} config.mk
	@echo LD $@
	$Q${LD} -o $@ ${OBJS} ${LDFLAGS}

%.o:%.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -c -o $@ $<


options:
	@echo "CC      = ${CC}"
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "PREFIX  = ${PREFIX}"

clean:
	${Q}rm -f *.o util/*.o ncurses/*.o uvi

.PHONY: clean options

buffer.o: buffer.c util/alloc.h range.h buffer.h util/list.h config.h
command.o: command.c config.h range.h buffer.h command.h util/list.h \
 vars.h util/alloc.h
main.o: main.c term.h ncurses/ncurses.h main.h config.h util/alloc.h
range.o: range.c range.h
term.o: term.c util/list.h range.h buffer.h term.h command.h vars.h \
 util/alloc.h config.h
vars.o: vars.c config.h util/alloc.h range.h buffer.h vars.h
marks.o: marks.c marks.h
motion.o: motion.c ncurses/../util/list.h ncurses/../range.h ncurses/../buffer.h motion.h marks.h
ncurses.o: ncurses.c ncurses/../config.h ncurses/../range.h ncurses/../buffer.h ncurses/../command.h \
 ncurses/../util/list.h ncurses/../main.h motion.h view.h ncurses/../util/alloc.h ncurses/../vars.h \
 marks.h ncurses.h
view.o: view.c ncurses/../util/list.h ncurses/../range.h ncurses/../buffer.h motion.h view.h \
 ncurses/../util/alloc.h ncurses/../config.h
view_colourscreen.o: view_colourscreen.c ncurses/../config.h ncurses/../util/list.h \
 ncurses/../range.h ncurses/../buffer.h view.h ncurses/../util/alloc.h
alloc.o: alloc.c alloc.h
list.o: list.c list.h alloc.h
