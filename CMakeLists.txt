cmake_minimum_required(VERSION 3.30)
project(scr2 C)

set(CMAKE_C_STANDARD 11)

add_executable(scr2 main.c client.c server.c)
