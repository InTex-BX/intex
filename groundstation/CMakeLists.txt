set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

add_library(intex_gui STATIC
  AspectRatioLayout.c++
  VideoWidget.c++
  VideoWindow.c++
  Control.c++
  VideoStreamControl.c++
)
qt5_use_modules(intex_gui Widgets Core)
set_target_properties(intex_gui PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(control main.c++)
target_link_libraries(control
  ${GSTREAMER_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${QTGSTREAMER_UI_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CAPNP_LIBRARIES}
  intex
  intex_gui
  c++abi
)

set_target_properties(control PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

