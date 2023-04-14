.POSIX:
.SUFFIXES:

CC = cc
CFLAGS = -D_POSIX_C_SOURCE -D_DEFAULT_SOURCE -DPL_POSIX -Wall  -Wextra \
         -I./vendor/secp256k1-0.3.1/include
DBG_CFLAGS = -g3 -O0 -DDEBUG_BUILD
RELEASE_CFLAGS = -O2 -DRELEASE_BUILD
PREFIX = /usr/local
LDFLAGS = -lm -L./vendor/secp256k1-0.3.1/.libs -l:libsecp256k1.a

all: ddfb

ddfb: *.c utils/*
	$(CC) $(CFLAGS) $(RELEASE_CFLAGS) -o ddfb ddfb.c $(LDFLAGS)

debug: *.c utils/*
	$(CC) $(CFLAGS) $(DBG_CFLAGS) -o ddfb-dbg ddfb.c $(LDFLAGS)

clean:
	rm -f ddfb ddfb.exe ddfb-dbg