#!/bin/sh

gcc -MM *.c ncurses/*.c util/*.c | sed 's/^\(ncurses\|view\)\.o/ncurses\/\1\.o/'
