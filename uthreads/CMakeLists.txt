cmake_minimum_required(VERSION 3.10)
project(uthreads C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(uthreads
    src/main1.c
    lib/uthreads.c
    lib/uthreads_wrappers.c
)