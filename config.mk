PREFIX  ?= /usr
VERBOSE = 0
DEBUG   = 0

ifeq (${DEBUG},1)
	CC      = gcc
	CFLAGS  = -g
	LDFLAGS = -g -lncurses
else
	CC      = tcc
	CFLAGS  = -Wall -Os
	LDFLAGS = -lncurses
endif
LD      = gcc
