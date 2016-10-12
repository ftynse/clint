#!/bin/sh
set -x
set -e

git submodule deinit -f .
git submodule init
git submodule update

if [ -z ${PREFIX+x} ]; then
  PREFIX=$HOME/usr
fi;

if [ -z ${QTPREFIX+x} ]; then
  QTPREFIX=/usr/lib
fi;

if [ -z ${JOBS+x} ]; then
  JOBS=8
fi;

cd isl
./autogen.sh
./configure --prefix=$PREFIX
make -j $JOBS && make install
cd -

cd openscop
./autogen.sh
./configure --prefix=$PREFIX
make -j $JOBS && make install
cd -

cd cloog
./autogen.sh
./configure --with-osl-builddir=../openscop \
            --with-isl-builddir=../isl \
            --prefix=$PREFIX
make -j $JOBS && make install
cd -

cd clan
./autogen.sh
./configure --with-osl-builddir=../openscop \
            --prefix=$PREFIX
make -j $JOBS && make install
cd -

cd piplib
./autogen.sh
./configure --with-gmp \
            --prefix=$PREFIX
make -j $JOBS && make install
cd -

cd candl
./autogen.sh
./configure --with-osl-builddir=../openscop \
            --with-piplib-builddir=../piplib \
            --prefix=$PREFIX
make -j $JOBS && make install
cd -

cd clay
./autogen.sh
./configure --with-osl-builddir=../openscop \
            --with-cloog-builddir=../cloog \
            --with-clan-builddir=../clan \
            --prefix=$PREFIX
make -j $JOBS && make install
cd -

mkdir chlore/build
cd chlore/build
cmake .. -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_INSTALL_PREFIX=$PREFIX
make -j $JOBS && make install
cd -

mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=$PREFIX\;$QTPATH -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_BUILD_TYPE=Release
make -j $JOBS && make install
cd -

