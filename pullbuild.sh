#!/bin/bash

git pull origin $(git rev-parse --abbrev-ref HEAD)

./build.sh
./dist/build/rc-boat
