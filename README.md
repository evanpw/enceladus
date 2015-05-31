# simple

A simple compiler for a simple language.

## Getting Started
* Make sure you have all of the requirements (see below)
* To build the compiler, just run make
* To run tests, run make tests. This should detect whether you're on OSX or Linux and run the appropriate build script.
* SPOILER ALERT: Tests consists mainly of solutions to Project Euler problems

## Requirements
* Boost
* nasm
* flex

## Features
* Syntax inspired by Python and Haskell
  * Blocks defined by indentation, a la Python
  * Noiseless function-call syntax, a la Haskell: "f x y" instead of "f(x, y)"
* Hindley-Milner type system with type inference
* Parametric polymorphism
* Unboxed Int and Bool types (through tagging)
* Mark-and-sweep garbage collection
* Local and global variables are immutable, but member of data structures are immutable
* Support for calling C functions
* Syntax highlighting for Sublime Text (see sublime/ directory)

## Compiler Stages
* Lexer, written in flex
* Lexer post-processing step: handles indentation and inserts INDENT and DEDENT tokens
* Parser: hand-coded recursive descent
* Semantic analysis: with global type inference
* Intermediate code generation: three-address style
* IR optimization
* Final codegen: x86_64, nasm syntax
