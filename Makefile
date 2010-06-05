include config.mk

LDFLAGS = -lncurses

VERBOSE ?= 0

ifeq (${VERBOSE},1)
	Q =
else
	Q = @
endif

OBJS = main.o term.o buffer.o range.o command.o vars.o \
	util/list.o util/alloc.o \
	ncurses/view.o ncurses/ncurses.o ncurses/motion.o ncurses/marks.o


uvi: ${OBJS} config.mk
	@echo LD $@
	$Q${LD} -o $@ ${OBJS} ${LDFLAGS}

options:
	@echo "CC      = ${CC}"
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "PREFIX  = ${PREFIX}"

%.o:%.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -c -o $@ $<

clean:
	${Q}rm -f *.o util/*.o ncurses/*.o uvi

.PHONY: clean options

buffer.o: buffer.c util/alloc.h range.h buffer.h util/list.h
command.o: command.c range.h buffer.h command.h util/list.h vars.h \
  util/alloc.h config.h
main.o: main.c term.h ncurses/ncurses.h main.h config.h
range.o: range.c range.h
term.o: term.c util/list.h range.h buffer.h term.h command.h vars.h \
  util/alloc.h config.h
vars.o: vars.c util/alloc.h range.h buffer.h vars.h config.h
alloc.o: util/alloc.c util/alloc.h
list.o: util/list.c util/list.h util/alloc.h
marks.o: ncurses/marks.c ncurses/marks.h
motion.o: ncurses/motion.c ncurses/motion.h
ncurses.o: ncurses/ncurses.c ncurses/../range.h ncurses/../buffer.h \
  ncurses/../command.h ncurses/../util/list.h ncurses/../main.h \
  ncurses/view.h ncurses/../util/alloc.h ncurses/../vars.h \
  ncurses/motion.h ncurses/marks.h ncurses/ncurses.h ncurses/../config.h
view.o: ncurses/view.c ncurses/../util/list.h ncurses/../range.h \
  ncurses/../buffer.h ncurses/view.h ncurses/../util/alloc.h
