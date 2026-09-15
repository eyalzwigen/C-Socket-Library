cmake_minimum_required(VERSION 4.2)
project(socket C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(socket
        include/socket/socket.h
        src/socket.c
)

target_include_directories(socket
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include

        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

target_sources(socket
        PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES
)

add_executable(test_client
        docs/examples/client.c
        docs/examples/server.c
)

target_link_libraries(test_client
        PRIVATE
        socket
)