cmake_minimum_required(VERSION 3.22)
project(communicate_2025_next VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

add_executable(communicate_2025_next
    example/serial_session.cpp
)

target_include_directories(communicate_2025_next PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(communicate_2025_next
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_compile_options(communicate_2025_next PRIVATE -Wall -Wextra -Wpedantic -Werror)


