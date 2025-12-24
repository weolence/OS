cmake_minimum_required(VERSION 3.10)
project(uthreads C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(uthreads
    src/main.c
    lib/uthreads.c
    lib/thread_local_storage.c
    lib/uthreads_queue.c
)