set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

add_library(intex_gui STATIC AspectRatioLayout.c++ VideoWidget.c++ VideoWindow.c++) 
qt5_use_modules(intex_gui Widgets Core)
set_target_properties(intex_gui PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

