CC=gcc
CFLAGS=-O1 -s -nostdlib -fno-stack-protector -Wall -std=c11 \
	-fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -Qn -Wl,--build-id=none

all: ay0i ooze

ay0i: ay0i.o substart.S syscall.S
	$(CC) $(CFLAGS) -o ay0i ay0i.c substart.S syscall.S

ooze: ooze.cpp
	g++ -g -std=c++14 -D_FILE_OFFSET_BITS=64 ooze.cpp -o ooze -lfuse


clean:
	rm -f *.o ay0i ooze

