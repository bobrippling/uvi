CFLAGS = -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs -Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline -Wredundant-decls -Wextra -pedantic -ansi

test: test.c regex.c regex.h
	${CC} ${CFLAGS} -o $@ test.c regex.c
