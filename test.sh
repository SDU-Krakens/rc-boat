#!/bin/bash

mkdir -p dist/test
cd dist/test

cmake -DCMAKE_BUILD_TYPE=Test -DBUILD_TESTS=ON ../../
make all
ctest --output-on-failure

cp ./compile_commands.json ../../compile_commands.json
