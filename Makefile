CFLAGS=-I/opt/local/include -I./h -I. -g -Wall -Wfatal-errors -Werror -Wno-switch -std=c++11

SOURCES=$(wildcard src/*.cpp)
HEADERS=$(wildcard src/*.hpp)
OBJECTS=$(patsubst src/%.cpp,build/%.o,$(SOURCES))

all: simplec tests

.PHONY: clean

clean:
	rm -rf simplec build

build/%.d: src/%.cpp
	@set -e; rm -f $@; mkdir -p build; \
	g++ -M $(CFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,build/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

simplec: build/lex.yy.o $(OBJECTS)
	g++ $(LDFLAGS) $^ -o simplec

-include $(patsubst src/%.cpp,build/%.d,$(SOURCES))

build/lex.yy.c: src/simple.flex
	flex -o build/lex.yy.c src/simple.flex

build/lex.yy.o: build/lex.yy.c
	g++ $(CFLAGS) -Wno-unused-function -Wno-deprecated-register -x c++ -c $< -o $@

build/%.o: src/%.cpp
	g++ $(CFLAGS) -c $< -o $@

tests: simplec
	testing/all.sh
