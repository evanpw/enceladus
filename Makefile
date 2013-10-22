CFLAGS=-I/opt/local/include -g -Wall -Wfatal-errors -std=c++11

SOURCES=$(wildcard *.cpp) lex.yy.c simple.tab.c
HEADERS=$(wildcard *.hpp) simple.tab.h

ifeq ($(shell uname -s),Darwin)
    CC=g++-4.8
endif

ifeq ($(shell uname -s),Linux)
    CC=g++
endif

all: simplec tests

.PHONY: clean

clean:
	rm -f simplec simple.tab.h simple.tab.c lex.yy.c

lex.yy.c: simple.flex
	flex simple.flex

simple.tab.c simple.tab.h: simple.y
	bison --report=state -d simple.y

simplec: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o simplec

tests: simplec
	testing/all.sh
