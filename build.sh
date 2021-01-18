#!/bin/sh
cmake -Bbuild -S.
if [ $? -ne 0 ]; then
    echo "CMake failed"
    return
fi
cd build
make -j
cd ..