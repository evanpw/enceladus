CFLAGS=-I/opt/local/include -g -Wall -Wfatal-errors -std=c++11

SOURCES=$(wildcard *.cpp)
HEADERS=$(wildcard *.hpp) simple.tab.h
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES))

ifeq ($(shell uname -s),Darwin)
    CC=g++-4.8
endif

ifeq ($(shell uname -s),Linux)
    CC=g++
endif

all: simplec tests

.PHONY: clean

clean:
	rm -f simplec simple.tab.h simple.tab.c lex.yy.c *.o

lex.yy.c: simple.flex
	flex simple.flex

simple.tab.c simple.tab.h: simple.y
	bison --report=state -d simple.y

lex.yy.o: lex.yy.c simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

simple.tab.o: simple.tab.c simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

simplec: $(OBJECTS) lex.yy.o simple.tab.o
	echo $^
	$(CC) $(LDFLAGS) $^ -o simplec

tests: simplec
	testing/all.sh
