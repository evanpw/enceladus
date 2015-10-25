add_library(semantic semantic.cpp symbol.cpp symbol_table.cpp types.cpp type_functions.cpp subtype.cpp unify_trait.cpp)
target_link_libraries(semantic ast)