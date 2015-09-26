add_subdirectory(rpc)

add_library(intex STATIC intex.c++)
set_target_properties(intex PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(intex intex_rpc)
qt5_use_modules(intex Core)

