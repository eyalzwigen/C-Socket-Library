cmake_minimum_required(VERSION 4.2)
project(c_socket_library C)

set(CMAKE_C_STANDARD 11)

add_library(c_socket_library SHARED library.c)
