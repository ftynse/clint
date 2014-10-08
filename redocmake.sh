#!/bin/sh

if [ ! -d build ]; then
  mkdir build;
fi

if [ ! -d release ]; then
  mkdir release;
fi

export CMAKE_PREFIX_PATH=$HOME/usr:$CMAKE_PREFIX_PATH
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
cd -

cd release
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd -
