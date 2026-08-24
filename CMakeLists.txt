cmake_minimum_required(VERSION 3.25)

project(xenon_host VERSION 0.1.0 LANGUAGES CXX)

# Clang is what this library is developed and tested with; AppleClang counts
# as Clang. Another compiler warns rather than bans -- a fresh machine must
# not have to install a specific vendor to try this library.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING
            "xenon-host is tested with clang++; configured compiler is "
            "${CMAKE_CXX_COMPILER_ID}, which has not been tested. Proceeding.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)

set(XENON_HOST_SOURCES
    src/digest.cpp
    src/guest_memory.cpp
    src/guest_memory_posix.cpp
    src/host.cpp
    src/module_validation.cpp
)

set(XENON_HOST_PUBLIC_HEADERS
    include/xenon_host/guest_module.hpp
    include/xenon_host/guest_memory.hpp
    include/xenon_host/host.hpp
    include/xenon_host/title_adapter.hpp
)

add_library(xenon_host STATIC ${XENON_HOST_SOURCES} ${XENON_HOST_PUBLIC_HEADERS})
target_include_directories(xenon_host PUBLIC include)
target_compile_options(xenon_host PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
enable_testing()

add_executable(xenon_host_contract_tests tests/contract_tests.cpp)
target_link_libraries(xenon_host_contract_tests PRIVATE xenon_host)
target_compile_options(xenon_host_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(xenon_host_contract_tests PRIVATE src)
add_test(NAME contract COMMAND xenon_host_contract_tests)

add_executable(xenon_host_guest_memory_tests tests/guest_memory_tests.cpp)
target_link_libraries(xenon_host_guest_memory_tests PRIVATE xenon_host)
target_compile_options(xenon_host_guest_memory_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(xenon_host_guest_memory_tests PRIVATE src)
add_test(NAME guest_memory COMMAND xenon_host_guest_memory_tests)

set(XENON_HOST_FORMAT_FILES
    ${XENON_HOST_SOURCES}
    ${XENON_HOST_PUBLIC_HEADERS}
    src/module_validation.hpp
    src/guest_memory_load.hpp
    src/host_run.hpp
    tests/contract_tests.cpp
    tests/guest_memory_tests.cpp
)

add_test(
    NAME format
    COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${XENON_HOST_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
    NAME clang_tidy
    COMMAND
        ${CLANG_TIDY_EXECUTABLE}
        -p=${CMAKE_BINARY_DIR}
        --warnings-as-errors=*
        ${XENON_HOST_SOURCES}
        tests/contract_tests.cpp
        tests/guest_memory_tests.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
    NAME structure
    COMMAND ${Python3_EXECUTABLE} tools/check_structure.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
    NAME structure_selftest
    COMMAND ${Python3_EXECUTABLE} tools/check_structure.py --selftest
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
endif()
