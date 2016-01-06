CC=gcc
CFLAGS=-Os -s -nostdlib -fno-stack-protector -Wall -std=c11 \
	-fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -Qn -Wl,--build-id=none

ay0i: ay0i.o substart.S syscall.S
	$(CC) $(CFLAGS) -o ay0i ay0i.c substart.S syscall.S

clean:
	rm -f *.o ay0i
