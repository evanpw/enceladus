add_library(semantic semantic.cpp symbol.cpp symbol_table.cpp types.cpp)
target_link_libraries(semantic ast)