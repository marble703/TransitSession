cmake_minimum_required(VERSION 3.22)
project(TransitSession VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(BOOST_ASIO_NO_DEPRECATED)

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

add_executable(TransitSessionWebSocket
    example/websocket_session.cpp
)

add_executable(TransitSessionWebSocketServer
    example/websocket_server.cpp
)

add_executable(TransitSessionWebSocketBenchmark
    example/websocket_benchmark.cpp
)

add_executable(TransitSessionSerialBenchmark
    example/serial_benchmark.cpp
)

add_executable(TransitSessionCanBenchmark
    example/can_benchmark.cpp
)

add_executable(TransitSessionCoreQueueTest
    test/core_queue_test.cpp
)

target_include_directories(TransitSession PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionCan PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionWebSocket PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionWebSocketServer PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionWebSocketBenchmark PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionSerialBenchmark PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionCanBenchmark PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(TransitSessionCoreQueueTest PRIVATE ${CMAKE_SOURCE_DIR})

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

target_link_libraries(TransitSessionWebSocket
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_link_libraries(TransitSessionWebSocketServer
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_link_libraries(TransitSessionWebSocketBenchmark
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_link_libraries(TransitSessionSerialBenchmark
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_link_libraries(TransitSessionCanBenchmark
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_link_libraries(TransitSessionCoreQueueTest
    PRIVATE
        Boost::system
        Boost::thread
        Threads::Threads
)

target_compile_options(TransitSession PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionCan PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionWebSocket PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionWebSocketServer PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionWebSocketBenchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionSerialBenchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionCanBenchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_options(TransitSessionCoreQueueTest PRIVATE -Wall -Wextra -Wpedantic -Werror)
