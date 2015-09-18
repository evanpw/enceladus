cmake_minimum_required(VERSION 2.8)
project("Simple")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/local/include -g -Wall -Wfatal-errors -Werror -Wno-switch -Wno-unused-function -Wno-deprecated-register -Wno-unused-but-set-variable -Wno-unused-variable -std=c++11")

include_directories(. ..)

add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(ast)
add_subdirectory(semantic)
add_subdirectory(ir)
add_subdirectory(codegen)

add_executable(simplec simplec.cpp)
target_link_libraries(simplec lexer parser ast semantic ir codegen)
