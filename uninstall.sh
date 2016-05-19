#!/bin/sh
set -x
set -e

if [ -z ${PREFIX+x} ]; then
  PREFIX=$HOME/usr
fi;

for i in isl openscop cloog clan candl clay; do
  cd $i;
  make uninstall;
  cd -;
done

cd chlore/build
make uninstall
cd -

cd build
make uninstall
cd -
