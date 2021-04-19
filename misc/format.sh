#!/bin/sh
git ls-files -- '*.cpp' '*.hpp' '*.c' '*.h' ':!:external/*' | xargs $(which clang-format) -i