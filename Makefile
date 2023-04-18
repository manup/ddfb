.POSIX:
.SUFFIXES:

CC = cc
CFLAGS = -DPL_POSIX -Wall  -Wextra \
         -I./vendor/micro-ecc
DBG_CFLAGS = -g3 -O0 -DDEBUG_BUILD
RELEASE_CFLAGS = -O2 -DRELEASE_BUILD
PREFIX = /usr/local
LDFLAGS = -lm

ifeq ($(findstring mingw32,$(CC)),mingw32)
    LDFLAGS += -lbcrypt
endif

all: ddfb

uecc.o:
	$(CC) -O2 -I./vendor/micro-ecc -c ./vendor/micro-ecc/uECC.c -o uecc.o

ddfb: *.c utils/* uecc.o
	$(CC) $(CFLAGS) $(RELEASE_CFLAGS) -o ddfb ddfb.c uecc.o $(LDFLAGS)

debug: *.c utils/* uecc.o
	$(CC) $(CFLAGS) $(DBG_CFLAGS) -o ddfb-dbg ddfb.c uecc.o $(LDFLAGS)

clean:
	rm -f ddfb ddfb.exe ddfb-dbg uecc.o
