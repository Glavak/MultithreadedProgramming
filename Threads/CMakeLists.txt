cmake_minimum_required(VERSION 3.8)
project(Threads)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS -pthread)

add_executable(Threads1-2 main1-2.c)
add_executable(Threads3 main3.c)
add_executable(Threads4-5 main4-5.c)
target_link_libraries(Threads4-5 m)

add_executable(Threads12 main12.c)
add_executable(Threads13 main13.c)