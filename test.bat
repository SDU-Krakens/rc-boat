@echo off

if not exist dist\test mkdir dist\test
cd dist\test

cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..\..\
cmake --build . -- /m
ctest --output-on-failure

if exist compile_commands.json copy compile_commands.json ..\..\

