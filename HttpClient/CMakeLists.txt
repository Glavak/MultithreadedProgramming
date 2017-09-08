cmake_minimum_required(VERSION 3.8)
project(HttpClient)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c)
add_executable(HttpClient ${SOURCE_FILES})