CFLAGS = -g -pipe -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs -Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline -Wredundant-decls -Wextra -pedantic -ansi

alloc.o: alloc.c alloc.h
buffer.o: buffer.c buffer.h list.h list.o
list.o: list.c list.h alloc.h alloc.o
