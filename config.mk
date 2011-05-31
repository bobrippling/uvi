WARN_EXTRA = -Wno-unused-parameter -Wno-char-subscripts

CC      = cc
CFLAGS  = -g -Wall -Wextra -pedantic -std=c99 ${WARN_EXTRA}

LD      = cc
LDFLAGS = -g -lncurses
