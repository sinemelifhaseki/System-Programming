Compilation:
gcc fs.c -o fs -ansi -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic -lansilove

Mount: ./fs -d src dst 
Unmount: fusermount -u dst