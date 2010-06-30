PREFIX  ?= /usr
VERBOSE = 0

# gcc
CC			?= gcc
LD			= gcc

LDFLAGS = -g -lncurses


ifeq (${CC},gcc)
	CFLAGS	= -g -pipe -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wnested-externs \
			-Waggregate-return -Wbad-function-cast -Wpointer-arith -Wcast-align \
			-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Winline \
			-Wno-char-subscript -Wredundant-decls -Wextra -pedantic -ansi -O1
else
	CFLAGS	= -g -Wimplicit-function-declaration -Wunsupported -Wwrite-strings -Wall
endif
