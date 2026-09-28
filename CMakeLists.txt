cmake_minimum_required(VERSION 3.23)

set(LIB_VERSION "0.0.0" CACHE STRING "Library version")

project(socket
        VERSION ${LIB_VERSION}
        LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT WIN32)
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
endif()

include(CTest)

option(ENABLE_SANITIZERS
        "Build with AddressSanitizer and UndefinedBehaviorSanitizer"
        OFF
)

option(ENABLE_VALGRIND
        "Run tests through Valgrind"
        OFF
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/include/socket/version.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/version.h
        @ONLY
)

add_library(socket
        src/socket.c
)

target_include_directories(socket
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

target_sources(socket
        PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/socket/socket.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/version.h
)

# Windows networking library
if(WIN32)
    target_link_libraries(socket PUBLIC ws2_32)
endif()

# Sanitizers
if(ENABLE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
                "ENABLE_SANITIZERS requires GCC or Clang"
        )
    endif()

    target_compile_options(socket
            PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -g
    )

    target_link_options(socket
            PRIVATE
            -fsanitize=address,undefined
    )
endif()

# Tests
if(BUILD_TESTING)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(CUnit
            REQUIRED
            IMPORTED_TARGET
            cunit
    )

    add_executable(test_socket
            tests/test_socket.c
    )

    target_link_libraries(test_socket
            PRIVATE
            socket
            PkgConfig::CUnit
    )

    add_test(
            NAME socket_tests
            COMMAND test_socket
    )

    if(ENABLE_VALGRIND)
        find_program(VALGRIND_EXECUTABLE
                NAMES valgrind
                REQUIRED
        )

        add_test(
                NAME socket_tests_valgrind
                COMMAND
                ${VALGRIND_EXECUTABLE}
                --leak-check=full
                --track-origins=yes
                --error-exitcode=1
                --quiet
                $<TARGET_FILE:test_socket>
        )
    endif()
endif()

# Installation
install(
        TARGETS socket
        EXPORT socketTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        FILE_SET public_headers
        DESTINATION include
)

install(
        EXPORT socketTargets
        FILE socketTargets.cmake
        NAMESPACE socket::
        DESTINATION lib/cmake/socket
)
