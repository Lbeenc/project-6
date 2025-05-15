# Makefile for CS4760 Assignment 6 - Memory Management Simulation

CC = gcc
CFLAGS = -Wall -g

all: oss user_proc

oss: oss.c
	$(CC) $(CFLAGS) -o oss oss.c

user_proc: user_proc.c
	$(CC) $(CFLAGS) -o user_proc user_proc.c

clean:
	rm -f *.o oss user_proc *.log
