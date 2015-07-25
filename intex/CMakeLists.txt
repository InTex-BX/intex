add_library(intex_video VideoStreamSourceControl.c++)
qt5_use_modules(intex_video Core)
set_target_properties(intex_video PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(intex_video 
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_VIDEO_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${QTGSTREAMER_LIBRARIES}
  intex
)

add_executable(experiment main.c++ IntexHardware.c++ CommandInterface.c++)
target_link_libraries(experiment
  intex_rpc
  intex_video
  ${CAPNP_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)
set_target_properties(experiment PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
qt5_use_modules(experiment Core Network)
