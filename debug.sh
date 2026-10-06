#!/bin/bash
set -e

cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug
