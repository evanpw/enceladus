add_library(semantic semantic.cpp symbol.cpp symbol_table.cpp types.cpp type_functions.cpp)
target_link_libraries(semantic ast)