# simple

A simple compiler for a simple language.

## Getting Started
* Make sure you have all of the requirements (see below)
* To build the compiler, run the script build.sh in the root directory of the project
* To run tests, run nosetests in the root directory. This should detect whether you're on OSX or Linux and run the appropriate build script.
* SPOILER ALERT: Tests consists mainly of solutions to Project Euler problems

## Requirements
* Boost
* nasm (don't use v2.11.08 on OSX)
* flex
* nosetests (for tests)

## Features
* Syntax inspired by Python, Haskell, and Rust
  * Blocks defined by indentation, a la Python
* Hindley-Milner type system with type inference
* Parametric polymorphism
* Unboxed, untagged integer and boolean types
* Cheney-style copying garbage collection
* Support for calling C functions
* Syntax highlighting for Sublime Text (see sublime/ directory)

## Compiler Stages
* Lexer, written in flex
* Lexer post-processing step: handles indentation and inserts INDENT and DEDENT tokens
* Parser: hand-coded recursive descent
* Semantic analysis: type inference within functions and at top level
* Intermediate code generation: SSA three-address code
* IR optimization
* Abstract machine-code generation, register allocation
* Final codegen: x86_64, nasm syntax
