#!/bin/bash
mkdir maps
chmod 777 maps
mkdir StormLib/build
cd StormLib/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DYNAMIC_MODULE=1 ..
make
make install
cd ../..
cd bncsutil/src/bncsutil/
make
make install
cd ../../..
make
make install
