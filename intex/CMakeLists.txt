add_library(sysfs sysfs.c++)
qt5_use_modules(sysfs Core)

add_library(intex_video VideoStreamSourceControl.c++)
qt5_use_modules(intex_video Core)
target_link_libraries(intex_video 
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_VIDEO_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${QTGSTREAMER_LIBRARIES}
  intex
  sysfs
)

add_library(intex_hardware IntexHardware.c++)
qt5_use_modules(intex_hardware Core)

add_executable(experiment main.c++ CommandInterface.c++ ExperimentControl.c++)
target_link_libraries(experiment
  intex_rpc
  intex_video
  intex_hardware
  ${CAPNP_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)
qt5_use_modules(experiment Core Network SerialPort)

add_executable(watchdog watchdog.c++)
target_link_libraries(watchdog intex_hardware)
qt5_use_modules(watchdog Core)
