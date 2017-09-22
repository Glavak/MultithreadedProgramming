cmake_minimum_required(VERSION 3.8)
project(PiCalculation)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_FLAGS -pthread)

add_executable(Finite finite.c)
add_executable(Infinite infinite.c)