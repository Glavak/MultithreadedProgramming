cmake_minimum_required(VERSION 3.8)
project(philosophers)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS -pthread)

set(SOURCE_FILES main.c)
add_executable(philosophers ${SOURCE_FILES})