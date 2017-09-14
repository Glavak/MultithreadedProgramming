cmake_minimum_required(VERSION 3.8)
project(PiCalculation)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_FLAGS -pthread)

set(SOURCE_FILES finite.c)
add_executable(Finite ${SOURCE_FILES})