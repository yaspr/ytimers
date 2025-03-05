CC=gcc
CFLAGS=-Wall -Wextra -g
OFLAGS=-O3
MFLAGS=-lm

all: ytimers

ytimers: ytimers.c
	$(CC) $(CFLAGS) $(OFLAGS) $< -o $@ $(MFLAGS)

clean:
	rm -Rf ytimers

