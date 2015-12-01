include_directories (${Enceladus_SOURCE_DIR}/src)

add_executable(runtests driver.cpp)
target_link_libraries(runtests semantic)
