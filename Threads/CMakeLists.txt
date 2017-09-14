cmake_minimum_required(VERSION 3.8)
project(Threads)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS -pthread)

set(SOURCE_FILES main1-2.c main3.c)
add_executable(Threads ${SOURCE_FILES})