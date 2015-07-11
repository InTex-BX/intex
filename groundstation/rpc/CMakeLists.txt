add_library(intex_client_rpc STATIC IntexRpcClient.c++ async-io.c++ ez-rpc.c++)
qt5_use_modules(intex_client_rpc Core)
target_link_libraries(intex_client_rpc intex_rpc)
set_target_properties(intex_client_rpc PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

