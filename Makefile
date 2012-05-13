all: lexer test_lexer

.PHONY: clean

clean:
	rm -f lexer simple.tab.h simple.tab.c lex.yy.c

lex.yy.c: simple.flex
	flex simple.flex
	
simple.tab.c simple.tab.h: simple.y
	bison -d simple.y

lexer: lexer.cpp lex.yy.c simple.tab.h string_table.hpp string_table.cpp
	g++ -g -Wall lexer.cpp lex.yy.c string_table.cpp -o lexer
	
test_lexer: lexer
	./test_lexer
	
parser: lex.yy.c simple.tab.h simple.tab.c string_table.hpp string_table.cpp memory_manager.hpp memory_manager.cpp ast.hpp ast.cpp utilities.hpp
	g++ -g -Wall lex.yy.c simple.tab.c string_table.cpp memory_manager.cpp ast.cpp -o parser