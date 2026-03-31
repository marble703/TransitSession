cmake_minimum_required(VERSION 3.22)
project(TransitSession VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build.")
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

add_executable(TransitSession
    example/serial_session.cpp
)

add_executable(TransitSessionCan
    example/can_session.cpp
)

target_include_directories(TransitSession PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionCan PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(TransitSession
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_link_libraries(TransitSessionCan
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_compile_options(TransitSession PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionCan PRIVATE -Wall -Wextra -Wpedantic -Werror)


