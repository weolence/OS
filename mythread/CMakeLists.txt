cmake_minimum_required(VERSION 3.10)
project(mythread C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Wall -Wextra -g -fsanitize=address -fsanitize=leak")

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(mythread
    src/main.c
    lib/mythread.c
)

target_link_libraries(mythread pthread)