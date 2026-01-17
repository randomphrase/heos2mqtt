cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(heos2mqtt VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

find_package(Threads REQUIRED)

find_package(Boost 1.90 REQUIRED COMPONENTS json mqtt5 beast)
find_package(Boost 1.90 QUIET COMPONENTS stacktrace_backtrace stacktrace_basic stacktrace_from_exception)
add_library(Boost::stacktrace INTERFACE IMPORTED)
if(TARGET Boost::stacktrace_backtrace)
    target_link_libraries(Boost::stacktrace INTERFACE Boost::stacktrace_backtrace)
elseif(TARGET Boost::stacktrace_basic)
    target_link_libraries(Boost::stacktrace INTERFACE Boost::stacktrace_basic)
endif()
if(TARGET Boost::stacktrace_from_exception)
    target_link_libraries(Boost::stacktrace INTERFACE Boost::stacktrace_from_exception)
endif()
if(TARGET Boost::stacktrace_backtrace)
    message(STATUS "Boost stacktrace backend: backtrace")
elseif(TARGET Boost::stacktrace_basic)
    message(STATUS "Boost stacktrace backend: basic")
else()
    message(STATUS "Boost stacktrace backend: none")
endif()
if(TARGET Boost::stacktrace_from_exception)
    message(STATUS "Boost stacktrace_from_exception: available")
else()
    message(STATUS "Boost stacktrace_from_exception: unavailable")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(lyra CONFIG REQUIRED)

find_package(Catch2 CONFIG REQUIRED)
include(Catch)


# asio

add_library(asio STATIC
    src/asio_separate_compilation.cpp
)
target_link_libraries(asio PUBLIC Boost::headers)
target_compile_definitions(asio PUBLIC
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_SEPARATE_COMPILATION
    BOOST_BEAST_SEPARATE_COMPILATION
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # On linux, we require liburing to enable io_uring support in asio
    find_library(HEOS2MQTT_LIBURING NAMES uring)
    if(HEOS2MQTT_LIBURING)
        target_link_libraries(asio PUBLIC ${HEOS2MQTT_LIBURING})
        target_compile_definitions(asio PUBLIC BOOST_ASIO_HAS_IO_URING=1)
    else()
        message(FATAL_ERROR "liburing is required on Linux builds to enable io_uring support.")
    endif()
endif()


# astest

add_library(astest INTERFACE)
target_sources(astest INTERFACE tests/catch_exception_stacktrace.cpp)
target_include_directories(astest INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(astest INTERFACE
    Boost::stacktrace
    logging
)
target_compile_definitions(astest INTERFACE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)

# logging

add_library(logging STATIC
    src/logging/logging.cpp
)
target_include_directories(logging PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(logging PUBLIC fmt::fmt)

add_executable(logging_tests
    tests/logging/logging_tests.cpp
)
target_link_libraries(logging_tests PRIVATE
    logging
    Catch2::Catch2WithMain
    astest
)
catch_discover_tests(logging_tests)

# ssdp

add_library(ssdp INTERFACE)
target_include_directories(ssdp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(ssdp INTERFACE
    asio
    Boost::beast
    logging
)

add_executable(ssdp_tests
    tests/ssdp/ssdp_resolver_tests.cpp
)
target_link_libraries(ssdp_tests PRIVATE
    ssdp
    Catch2::Catch2WithMain
    Boost::headers
    astest
)
catch_discover_tests(ssdp_tests)

# heos

add_library(heos_client STATIC
    src/heos_client.cpp
)
target_include_directories(heos_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(heos_client
    PUBLIC
    asio
    Boost::headers
    ssdp
    logging
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
    asio
    Boost::headers
    Boost::json
    fmt::fmt
    Threads::Threads
)

add_executable(heos2mqtt
    src/heos2mqtt.cpp
)
target_link_libraries(heos2mqtt PRIVATE heos_client mqtt_publisher)

add_executable(ssdp_query
    src/ssdp_query.cpp
)
target_link_libraries(ssdp_query PRIVATE
    asio
    ssdp
    bfg::lyra
    fmt::fmt
)

add_executable(heos_tests
    tests/heos_client/heos_client_tests.cpp
)
target_link_libraries(heos_tests
    PRIVATE
    heos_client
    Catch2::Catch2WithMain
    Boost::headers
    ssdp
    astest
)



catch_discover_tests(heos_tests)
