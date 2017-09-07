cmake_minimum_required(VERSION 3.8)
project(HttpServer)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c addressUtils.c logging.c connection.c connection.h common.h)
add_executable(HttpServer ${SOURCE_FILES})