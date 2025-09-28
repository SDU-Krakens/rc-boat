#!/bin/bash

BUILD_TYPE=${1:-Debug}

mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..
make

cp ./compile_commands.json ../

echo "Running 'rc-boat'"
./rc-boat
