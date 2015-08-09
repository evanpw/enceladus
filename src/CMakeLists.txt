cmake_minimum_required(VERSION 2.8)
project("Simple")
include_directories("../h")
include_directories("..")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/local/include -g -Wall -Wfatal-errors -Werror -Wno-switch -Wno-unused-function -Wno-deprecated-register -std=c++11")

find_package(FLEX)
FLEX_TARGET(Scanner simple.flex ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp)

add_executable(simplec asm_printer.cpp ast_context.cpp ast.cpp ast_visitor.cpp basic_block.cpp context.cpp from_ssa.cpp function.cpp lexer_transform.cpp machine_codegen.cpp machine_context.cpp machine_instruction.cpp mangler.cpp parser.cpp redundant_moves.cpp reg_alloc.cpp semantic.cpp simplec.cpp string_table.cpp symbol.cpp symbol_table.cpp tac_codegen.cpp tac_instruction.cpp tac_validator.cpp tokens.cpp to_ssa.cpp types.cpp utility.cpp value.cpp stack_map.cpp constant_folding.cpp demote_globals.cpp tag_elision.cpp kill_dead_values.cpp ${FLEX_Scanner_OUTPUTS})
