include config.mk

VERBOSE ?= 0

ifeq (${VERBOSE},1)
	Q =
else
	Q = @
endif

OBJS = main.o term.o buffer.o range.o command.o vars.o \
	util/list.o util/alloc.o util/io.o util/pipe.o \
	view/view_raw.o view/ncurses.o view/motion.o \
	view/marks.o


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

