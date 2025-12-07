#!/bin/bash

mkdir -p build && cd build
cmake .. && make
cp mythread ../mythread