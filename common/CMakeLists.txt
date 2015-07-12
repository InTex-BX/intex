add_subdirectory(rpc)

add_library(intex STATIC intex.c++)
set_target_properties(intex PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

