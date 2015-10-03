include_directories(. ..)

add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(ast)
add_subdirectory(semantic)
add_subdirectory(ir)
add_subdirectory(codegen)

add_executable(simplec simplec.cpp)
target_link_libraries(simplec lexer parser ast semantic ir codegen)
