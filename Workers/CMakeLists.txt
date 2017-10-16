cmake_minimum_required(VERSION 3.8)
project(Workers)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS -pthread)

set(SOURCE_FILES main.c)
add_executable(Workers ${SOURCE_FILES})