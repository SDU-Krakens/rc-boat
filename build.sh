#!/bin/bash

BUILD_TYPE=${1:-Release}

mkdir -p dist/build
cd dist/build

cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ../../
make

cp ./compile_commands.json ../
