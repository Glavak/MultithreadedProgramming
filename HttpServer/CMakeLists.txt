cmake_minimum_required(VERSION 3.8)
project(HttpServer)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES addressUtils.c cacheEntry.c httpUtils.c logging.c connection.c connection.h common.h)
add_executable(Proxy ${SOURCE_FILES} main.c)
add_executable(Test ${SOURCE_FILES} tests.c)