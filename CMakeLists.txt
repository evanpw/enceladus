cmake_minimum_required(VERSION 2.8)
project("Enceladus")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/local/include -g -Wall -Wfatal-errors -Werror -Wno-switch -Wno-unused-function -Wno-deprecated-register -Wno-unused-variable -std=c++11")

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-private-field")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
endif()

add_subdirectory(src)
add_subdirectory(unittests)
