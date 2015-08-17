This repository contains the control software of the InTex experiment.

Prerequisites:

- Qt5
- libgstreamer
- qt-gstreamer
  - boost (optional?)
- capnp

Build:

cd <intex-main-dir>
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/qt5 -DCMAKE_C_COMPILER=/usr/local/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/bin/clang++
make
