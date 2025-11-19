#!/bin/bash

mkdir -p dist/test
cd dist/test

cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ../../
cmake --build . -- -j$(nproc)

echo "=== FILES IN BUILD DIR ==="
find . -type f -executable

cp ./compile_commands.json ../../compile_commands.json

ctest -V --output-on-failure
exit $?
