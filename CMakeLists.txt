cmake_minimum_required(VERSION 3.5)
project(smart_layout_daemon)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

set(SOURCE_FILES main.c lru.c)

add_executable(smart_layout_daemon ${SOURCE_FILES})

add_executable(lru_test lru.c lru_test.c)

target_link_libraries(smart_layout_daemon X11)
