CC=g++
CFLAGS=-I/opt/local/include -I./h -I. -g -Wall -Wfatal-errors -Werror -Wno-switch -std=c++11

SOURCES=$(wildcard src/*.cpp)
HEADERS=$(wildcard src/*.hpp)
OBJECTS=$(patsubst src/%.cpp,build/%.o,$(SOURCES))

all: simplec

.PHONY: clean

clean:
	rm -rf simplec build

build/%.d: src/%.cpp
	@set -e; rm -f $@; mkdir -p build; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,build/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

simplec: build/lex.yy.o $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o simplec

-include $(patsubst src/%.cpp,build/%.d,$(SOURCES))

build/lex.yy.c: src/simple.flex
	flex -o build/lex.yy.c src/simple.flex

build/lex.yy.o: build/lex.yy.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-deprecated-register -x c++ -c $< -o $@

build/%.o: src/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

tests: simplec
	python testing/test.py
