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

buffer.o: buffer.c util/alloc.h range.h buffer.h util/list.h util/io.h
command.o: command.c range.h buffer.h command.h util/list.h vars.h \
 util/alloc.h util/pipe.h global.h
global.o: global.c range.h buffer.h global.h
main.o: main.c main.h range.h buffer.h global.h
range.o: range.c range.h
vars.o: vars.c util/alloc.h range.h buffer.h vars.h global.h
base.o: gui/base.c gui/../range.h gui/../buffer.h gui/../command.h \
 gui/../util/list.h gui/../global.h gui/motion.h gui/../util/alloc.h \
 gui/gui.h gui/marks.h
gui.o: gui/gui.c gui/../range.h gui/../util/list.h gui/../buffer.h \
 gui/motion.h gui/gui.h gui/../global.h gui/../config.h
marks.o: gui/marks.c gui/marks.h
motion.o: gui/motion.c gui/../range.h gui/../util/list.h gui/../buffer.h \
 gui/motion.h gui/gui.h gui/marks.h gui/../global.h
alloc.o: util/alloc.c util/alloc.h
io.o: util/io.c util/alloc.h util/io.h
list.o: util/list.c util/list.h util/alloc.h
pipe.o: util/pipe.c util/list.h util/io.h util/pipe.h util/alloc.h
