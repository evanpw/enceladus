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
	rm -f simplec build/*

build/%.d: %.cpp
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,build/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(patsubst %.cpp,build/%.d,$(SOURCES))

build/lex.yy.c: simple.flex
	flex -o build/lex.yy.c simple.flex

build/simple.tab.c build/simple.tab.h: simple.y
	bison -o build/simple.tab.c -d simple.y

build/lex.yy.o: build/lex.yy.c simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

build/simple.tab.o: build/simple.tab.c build/simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.cpp build/simple.tab.h
	$(CC) $(CFLAGS) -c $< -o $@

simplec: $(OBJECTS) build/lex.yy.o build/simple.tab.o
	echo $^
	$(CC) $(LDFLAGS) $^ -o simplec

tests: simplec
	testing/all.sh
