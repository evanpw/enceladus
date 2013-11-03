CFLAGS=-I/opt/local/include -I. -g -Wall -Wfatal-errors -std=c++11

SOURCES=$(wildcard *.cpp)
HEADERS=$(wildcard *.hpp) simple.tab.h
OBJECTS=$(patsubst %.cpp,build/%.o,$(SOURCES))

ifeq ($(shell uname -s),Darwin)
    CC=g++-4.8
endif

ifeq ($(shell uname -s),Linux)
    CC=g++
endif

all: simplec tests

.PHONY: clean

clean:
	rm -f simplec build/* lex.yy.c simple.tab.c simple.tab.h

build/%.d: %.cpp
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,build/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

simplec: build/simple.tab.o build/lex.yy.o $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o simplec

-include $(patsubst %.cpp,build/%.d,$(SOURCES))

lex.yy.c: simple.flex
	flex -o lex.yy.c simple.flex

simple.tab.c simple.tab.h: simple.y
	bison -d simple.y

build/lex.yy.o: lex.yy.c simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

build/simple.tab.o: simple.tab.c simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.cpp simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

tests: simplec
	testing/all.sh
