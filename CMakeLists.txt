cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(heos2mqtt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

find_package(Boost 1.90 REQUIRED COMPONENTS json mqtt5 stacktrace_backtrace)
find_package(Boost 1.90 QUIET COMPONENTS stacktrace_from_exception)
if(NOT TARGET Boost::stacktrace AND TARGET Boost::stacktrace_backtrace)
  add_library(Boost::stacktrace INTERFACE IMPORTED)
  target_link_libraries(Boost::stacktrace INTERFACE Boost::stacktrace_backtrace)
endif()

find_package(fmt CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(HEOS2MQTT_HAS_IO_URING OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_library(HEOS2MQTT_LIBURING NAMES uring)
  if(HEOS2MQTT_LIBURING)
    set(HEOS2MQTT_HAS_IO_URING ON)
  else()
    message(FATAL_ERROR "liburing is required on Linux builds to enable io_uring support.")
  endif()
endif()

add_library(logging STATIC
    src/logging/logging.cpp
)
target_include_directories(logging PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(logging PUBLIC fmt::fmt)
add_executable(logging_tests
    tests/logging_tests.cpp
)
target_link_libraries(logging_tests PRIVATE
  logging
  Catch2::Catch2WithMain
)

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
        logging
        fmt::fmt
        Threads::Threads
)
if(HEOS2MQTT_HAS_IO_URING)
  target_link_libraries(heos_client PUBLIC ${HEOS2MQTT_LIBURING})
  target_compile_definitions(heos_client PUBLIC BOOST_ASIO_HAS_IO_URING=1)
endif()

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
    tests/catch_exception_stacktrace.cpp
    tests/heos_client_tests.cpp
    tests/logging_tests.cpp
    tests/ssdp_resolver_tests.cpp
)
target_link_libraries(heos_client_tests
    PRIVATE
        heos_client
        Catch2::Catch2WithMain
        Boost::headers
        Boost::stacktrace_backtrace
        logging
)
if(TARGET Boost::stacktrace_from_exception)
    target_link_libraries(heos_client_tests PRIVATE Boost::stacktrace_from_exception)
endif()
target_compile_definitions(heos_client_tests PRIVATE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)

include(CTest)
include(Catch)
catch_discover_tests(heos_client_tests)
