cmake_minimum_required(VERSION 2.8)
project("Simple")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/local/include -g -Wall -Wfatal-errors -Werror -Wno-switch -Wno-unused-function -Wno-deprecated-register -Wno-unused-but-set-variable -Wno-unused-variable -std=c++11")

add_subdirectory(src)
add_subdirectory(unittests)
