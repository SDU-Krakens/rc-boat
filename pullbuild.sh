#!/bin/bash

git pull origin $(git rev-parse --abbrev-ref HEAD)

./build.sh
./build/rc-boat
