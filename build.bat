@echo off

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

if not exist dist\build mkdir dist\build
cd dist\build

cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..\..\
cmake --build . -- /m

copy compile_commands.json ..\

