include config.mk

OBJ = main.o buffer.o buffers.o range.o command.o vars.o \
	util/list.o util/alloc.o util/io.o util/pipe.o util/str.o util/term.o util/search.o \
	gui/gui.o gui/motion.o gui/marks.o gui/base.o gui/intellisense.o \
	gui/map.o gui/macro.o gui/visual.o gui/syntax.o \
	global.o rc.o preserve.o yank.o


uvi: ${OBJ} config.mk
	@echo LD $@
	@${LD} -o $@ ${OBJ} ${LDFLAGS}

.c.o:
	@echo CC $<
	@${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f uvi `find . -iname \*.o`

install: uvi
	cp uvi ${PREFIX}/bin

uninstall:
	rm -f ${PREFIX}/bin/uvi

.PHONY: clean

# :r!for d in . util gui; do cc -MM $d/*.c | sed "s;^[^ \t];$d/&;"; done
./buffer.o: buffer.c util/alloc.h range.h buffer.h util/list.h util/io.h \
 global.h
./buffers.o: buffers.c range.h buffer.h buffers.h gui/gui.h util/io.h \
 util/alloc.h
./command.o: command.c range.h buffer.h command.h util/list.h vars.h \
 util/alloc.h util/pipe.h global.h gui/motion.h gui/intellisense.h \
 gui/gui.h util/io.h yank.h buffers.h util/str.h rc.h gui/map.h config.h \
 gui/marks.h bloat/command.c bloat/command.h
./global.o: global.c range.h buffer.h global.h
./main.o: main.c main.h range.h buffer.h global.h gui/motion.h \
 gui/intellisense.h gui/gui.h rc.h command.h util/io.h preserve.h \
 gui/map.h util/alloc.h util/str.h util/list.h buffers.h
./preserve.o: preserve.c range.h buffer.h preserve.h util/alloc.h
./range.o: range.c range.h
./rc.o: rc.c rc.h range.h buffer.h vars.h global.h util/io.h gui/map.h \
 buffers.h
./vars.o: vars.c util/alloc.h range.h buffer.h vars.h global.h gui/motion.h \
 gui/intellisense.h gui/gui.h buffers.h
./yank.o: yank.c util/alloc.h range.h util/list.h yank.h
util/alloc.o: util/alloc.c util/alloc.h util/../main.h
util/io.o: util/io.c util/alloc.h util/../range.h util/../buffer.h util/io.h \
 util/../main.h util/../gui/motion.h util/../gui/intellisense.h \
 util/../gui/gui.h util/../util/list.h
util/list.o: util/list.c util/../range.h util/list.h util/alloc.h
util/pipe.o: util/pipe.c util/../range.h util/list.h util/io.h util/pipe.h \
 util/alloc.h
util/search.o: util/search.c util/search.h util/alloc.h util/../global.h
util/str.o: util/str.c util/../range.h util/list.h util/str.h util/alloc.h
util/term.o: util/term.c
gui/base.o: gui/base.c gui/../range.h gui/../buffer.h gui/../command.h \
 gui/../util/list.h gui/../global.h gui/motion.h gui/../util/alloc.h \
 gui/intellisense.h gui/gui.h gui/macro.h gui/marks.h gui/../main.h \
 gui/../util/str.h gui/../yank.h gui/map.h gui/../buffers.h gui/visual.h \
 gui/../util/search.h
gui/gui.o: gui/gui.c gui/../range.h gui/../util/list.h gui/../buffer.h \
 gui/motion.h gui/intellisense.h gui/gui.h gui/../global.h \
 gui/../util/alloc.h gui/../util/str.h gui/../util/term.h \
 gui/../util/io.h gui/macro.h gui/marks.h gui/../buffers.h gui/visual.h \
 gui/../yank.h gui/../util/search.h
gui/intellisense.o: gui/intellisense.c gui/intellisense.h gui/../range.h \
 gui/../buffer.h gui/../global.h gui/../util/str.h gui/../util/list.h \
 gui/../util/alloc.h gui/motion.h gui/gui.h gui/../buffers.h
gui/macro.o: gui/macro.c gui/../range.h gui/../buffer.h gui/gui.h gui/macro.h
gui/map.o: gui/map.c gui/../range.h gui/../buffer.h gui/gui.h gui/map.h \
 gui/../util/list.h gui/../util/alloc.h gui/../util/str.h
gui/marks.o: gui/marks.c gui/marks.h gui/gui.h
gui/motion.o: gui/motion.c gui/../range.h gui/../util/list.h gui/../buffer.h \
 gui/motion.h gui/intellisense.h gui/gui.h gui/marks.h gui/../global.h \
 gui/../util/str.h gui/../buffers.h
gui/visual.o: gui/visual.c gui/../range.h gui/gui.h gui/visual.h
