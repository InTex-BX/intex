set(CAPNPC_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS intex.capnp)

add_library(intex_rpc STATIC async-io.c++ ez-rpc.c++ ${CAPNP_SRCS})
qt5_use_modules(intex_rpc Core)

