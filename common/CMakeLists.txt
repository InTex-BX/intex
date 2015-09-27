add_subdirectory(rpc)

add_library(intex STATIC intex.c++)
target_link_libraries(intex intex_rpc)
qt5_use_modules(intex Core)

