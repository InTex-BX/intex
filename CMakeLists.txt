project(Intex)
cmake_minimum_required(VERSION 2.8)

if(NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  message(FATAL "Compilers other than clang are not supported yet.")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")
if(${GCC_TOOLCHAIN})
  # Not really a definition, but neither a language flag for C/CXX_FLAGS
  add_definitions(-gcc-toolchain=${GCC_TOOLCHAIN})
endif()

if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "arm")
  add_definitions(-DBUILD_ON_RASPBERRY)
  message(STATUS "Building live experiment system (Raspberry)")
else()
  message(STATUS "Building debug experiment system (non-Raspberry)")
endif()

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost COMPONENTS system REQUIRED)

find_package(Qt5 COMPONENTS Widgets Core Network REQUIRED)

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
include_directories(${CMAKE_CURRENT_BINARY_DIR}/common)

add_subdirectory(common)
add_subdirectory(groundstation)
add_subdirectory(intex)
