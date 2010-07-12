#!/bin/sh

set -e

gcc -MM *.c

for d in ncurses util
do
	cd $d
	gcc -MM *.c | sed "s/\(^\| \)/\1$d\//g"
	cd ..
done
