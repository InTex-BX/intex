set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

add_library(intex_gui STATIC
  AspectRatioLayout.c++
  VideoWidget.c++
  VideoWindow.c++
  VideoStreamControl.c++
  IntexWidget.c++
)

qt5_use_modules(intex_gui Widgets Core)
set_target_properties(intex_gui PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(control main.c++ Control.c++ IntexRpcClient.c++)
target_link_libraries(control
  ${GSTREAMER_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${QTGSTREAMER_UI_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CAPNP_LIBRARIES}
  intex
  intex_gui
  intex_rpc
  c++abi
)

set_target_properties(control PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

