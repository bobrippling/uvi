include config.mk

LDFLAGS = -lncurses

VERBOSE ?= 0

ifeq (${VERBOSE},1)
	Q =
else
	Q = @
endif


uvi: main.o term.o buffer.o range.o command.o vars.o \
	util/list.o util/alloc.o \
	ncurses/view.o ncurses/ncurses.o ncurses/motion.o
	@echo LD $@
	$Q${LD} -o $@ $^ ${LDFLAGS}

options:
	@echo CC: ${CC}
	@echo CFLAGS: ${CFLAGS}
	@echo LDFLAGS: ${LDFLAGS}

%.o:%.c
	@echo CC $@
	$Q${CC} ${CFLAGS} -c -o $@ $<

clean:
	${Q}rm -f *.o util/*.o ncurses/*.o uvi

.PHONY: clean

