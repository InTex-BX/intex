add_library(intex_gui STATIC
  AspectRatioLayout.c++
  VideoWidget.c++
  VideoWindow.c++
  VideoStreamControl.c++
  IntexWidget.c++
)

qt5_use_modules(intex_gui Widgets Core)
set_target_properties(intex_gui PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(intex_gui
  ${GSTREAMER_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${QTGSTREAMER_UI_LIBRARIES}
)

add_executable(control main.c++ Control.c++ IntexRpcClient.c++)
target_link_libraries(control
  ${CMAKE_THREAD_LIBS_INIT}
  intex
  intex_gui
  intex_rpc
  ${CAPNP_LIBRARIES}
)

qt5_use_modules(control Network)
set_target_properties(control PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

