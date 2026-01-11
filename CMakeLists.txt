cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(heos2mqtt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

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
find_package(Catch2 CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(asio STATIC
    src/asio_separate_compilation.cpp
)
target_link_libraries(asio PUBLIC Boost::headers)
target_compile_definitions(asio PUBLIC
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_SEPARATE_COMPILATION
)

# On linux, we require liburing to enable io_uring support in asio
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_library(HEOS2MQTT_LIBURING NAMES uring)
  if(HEOS2MQTT_LIBURING)
    target_link_libraries(asio PUBLIC ${HEOS2MQTT_LIBURING})
    target_compile_definitions(asio PUBLIC BOOST_ASIO_HAS_IO_URING=1)
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
add_library(test_support INTERFACE)
target_sources(test_support INTERFACE tests/catch_exception_stacktrace.cpp)
target_link_libraries(test_support INTERFACE Boost::stacktrace)

target_link_libraries(logging_tests PRIVATE
  logging
  Catch2::Catch2WithMain
  test_support
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
        asio
        Boost::headers
        Boost::beast
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
    src/main.cpp
)
target_link_libraries(heos2mqtt PRIVATE heos_client mqtt_publisher)

add_executable(heos_client_tests
    tests/heos_client_tests.cpp
    tests/logging_tests.cpp
    tests/ssdp_resolver_tests.cpp
)
target_link_libraries(heos_client_tests
    PRIVATE
        heos_client
        Catch2::Catch2WithMain
        Boost::headers
        logging
        test_support
)
target_compile_definitions(heos_client_tests PRIVATE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)

include(CTest)
include(Catch)
catch_discover_tests(heos_client_tests)
