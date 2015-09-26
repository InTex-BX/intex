project(Intex)
cmake_minimum_required(VERSION 2.8)

if(NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  message(FATAL_ERROR "Compilers other than clang are not supported yet.")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")
if(NOT "${GCC_TOOLCHAIN}" STREQUAL "")
  if(NOT EXISTS "${GCC_TOOLCHAIN}")
    message(FATAL_ERROR "GCC toolchain directory ${GCC_TOOLCHAIN} does not exist.")
  endif()
  set(TOOLCHAIN "-gcc-toolchain ${GCC_TOOLCHAIN}")
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
find_package(Boost COMPONENTS system program_options REQUIRED)

find_package(Qt5 COMPONENTS Widgets Core Network REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

find_package(GStreamer REQUIRED)
find_package(GObject REQUIRED)
find_package(Qt5GStreamer REQUIRED)
find_package(CapnProto REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_VIDEO_INCLUDE_DIRS})
include_directories(${GOBJECT_INCLUDE_DIRS})
include_directories(${QTGSTREAMER_INCLUDES})
include_directories(${CAPNP_INCLUDE_DIRS})

add_definitions(${Boost_DEFINITIONS})
add_definitions(${QTGSTREAMER_DEFINITIONS})
add_definitions(${CAPNP_DEFINITIONS})
add_definitions(-DCAPNP_HEADER_WARNINGS=0)
add_definitions(-DKJ_HEADER_WARNINGS=0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(common)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/common)

function(add_library name)
  set_property(GLOBAL APPEND PROPERTY INTEX_TARGETS ${name})
  _add_library(${name} ${ARGN})
endfunction()

function(add_executable name)
  set_property(GLOBAL APPEND PROPERTY INTEX_TARGETS ${name})
  _add_executable(${name} ${ARGN})
endfunction()

add_subdirectory(common)
add_subdirectory(groundstation)
add_subdirectory(intex)

get_property(INTEX_TARGETS GLOBAL PROPERTY INTEX_TARGETS)
set_property(TARGET ${INTEX_TARGETS} APPEND PROPERTY COMPILE_FLAGS ${TOOLCHAIN})
set_property(TARGET ${INTEX_TARGETS} APPEND PROPERTY LINK_FLAGS ${TOOLCHAIN})
