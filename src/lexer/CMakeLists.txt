find_package(FLEX)
FLEX_TARGET(Scanner lexer.flex ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp)

add_library(lexer ${FLEX_Scanner_OUTPUTS} lexer_transform.cpp string_table.cpp)
