cmake_minimum_required(VERSION 3.24)
project(heos2mqtt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

find_package(Boost 1.90 REQUIRED COMPONENTS json mqtt5)
find_package(fmt CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(heos_client STATIC
    src/heos_client.cpp
)
target_include_directories(heos_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(heos_client
    PUBLIC
        Boost::headers
        fmt::fmt
        Threads::Threads
)

add_library(mqtt_publisher STATIC
    src/mqtt_publisher.cpp
)
target_include_directories(mqtt_publisher PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mqtt_publisher
    PUBLIC
        Boost::headers
        Boost::json
        fmt::fmt
        Threads::Threads
)

add_executable(heos2mqtt
    src/main.cpp
)
target_link_libraries(heos2mqtt PRIVATE heos_client mqtt_publisher)

add_executable(heos_client_tests
    tests/heos_client_tests.cpp
    tests/ssdp_resolver_tests.cpp
)
target_link_libraries(heos_client_tests
    PRIVATE
        heos_client
        Catch2::Catch2WithMain
        Boost::headers
        fmt::fmt
)

include(CTest)
include(Catch)
catch_discover_tests(heos_client_tests)
