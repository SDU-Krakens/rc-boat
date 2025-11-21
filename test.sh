#!/bin/bash
set -ex

mkdir -p dist/test
cd dist/test

cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ../../
cmake --build . -- -j$(nproc)

cp ./compile_commands.json ../../compile_commands.json

ctest -V --output-on-failure
exit $?
