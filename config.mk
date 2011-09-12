WARN_EXTRA = -Wno-unused-parameter -Wno-char-subscripts
PREFIX     = /usr

# if on a FreeBSD derived system (inc. Mac OS), add -DUVI_ALLOCA
MACROS     = -D_POSIX_SOURCE -D_GNU_SOURCE

CC      = cc
CFLAGS  = -g -Wall -Wextra -pedantic -std=c99 ${MACROS} ${WARN_EXTRA}

LD      = cc
LDFLAGS = -g -lncurses
