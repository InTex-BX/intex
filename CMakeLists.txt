project(Intex)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(APPLE)
  set(Qt5_DIR "/Applications/Qt/5.4/clang_64/lib/cmake/Qt5")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic")

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost COMPONENTS system REQUIRED)

find_package(Qt5 COMPONENTS Widgets Core REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

find_package(GStreamer REQUIRED)
find_package(GObject REQUIRED)
find_package(Qt5GStreamer REQUIRED)
find_package(CapnProto REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GOBJECT_INCLUDE_DIRS})
include_directories(${QTGSTREAMER_INCLUDES})
include_directories(${CAPNP_INCLUDE_DIRS})

add_definitions(${Boost_DEFINITIONS})
add_definitions(${QTGSTREAMER_DEFINITIONS})
add_definitions(${CAPNP_DEFINITIONS})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(common)

add_subdirectory(common)
