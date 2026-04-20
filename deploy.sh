#!/bin/bash
set -e
cd "$(dirname "$0")"
git fetch --all
git reset --hard origin/main
cmake -B build
cmake --build build
systemctl restart boat
echo "Done. Binary: build/rc-boat"
