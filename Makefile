ifeq ($(shell uname -s),Darwin)
    CC=g++-4.8
endif

all: parser

.PHONY: clean

clean:
	rm -f lexer simple.tab.h simple.tab.c lex.yy.c

lex.yy.c: simple.flex
	flex simple.flex

simple.tab.c simple.tab.h: simple.y
	bison -d simple.y

parser: lex.yy.c simple.tab.h simple.tab.c string_table.hpp string_table.cpp memory_manager.hpp memory_manager.cpp ast.hpp ast.cpp utilities.hpp symbol_table.hpp symbol_table.cpp parser.cpp semantic.hpp semantic.cpp ast_visitor.hpp ast_visitor.cpp codegen.hpp codegen.cpp types.hpp types.cpp lexer_transform.cpp
	$(CC) -I/opt/local/include -g -Wall -Wfatal-errors -std=c++0x lex.yy.c simple.tab.c string_table.cpp memory_manager.cpp ast.cpp symbol_table.cpp parser.cpp semantic.cpp ast_visitor.cpp codegen.cpp types.cpp lexer_transform.cpp -o parser
