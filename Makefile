include config.mk

OBJS = main.o buffer.o range.o command.o vars.o \
	util/list.o util/alloc.o util/io.o util/pipe.o \
	gui/gui.o gui/motion.o gui/marks.o gui/base.o \
	global.o


uvi: ${OBJS} config.mk
	${LD} -o $@ ${OBJS} ${LDFLAGS}

%.o:%.c
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	${Q}rm -f uvi `find -iname \*.o`

.PHONY: clean
