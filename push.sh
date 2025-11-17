#!/bin/bash

git add .
git commit -m "fast push"
git push origin $(git rev-parse --abbrev-ref HEAD)
