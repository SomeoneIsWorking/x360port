cmake_minimum_required(VERSION 3.25)

project(x360port VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)

set(X360PORT_VALIDATION_SOURCES
    src/digest.cpp
    src/module_validation.cpp
    src/validation.cpp
)
set(X360PORT_VALIDATION_HEADERS
    include/x360port/module_contract.hpp
    include/x360port/validation.hpp
)

# This is deliberately not named x360port: no executor target exists until
# Xenia Memory, Processor, ThreadState, and RawModule are actually embedded.
add_library(x360port_validation STATIC
    ${X360PORT_VALIDATION_SOURCES}
    ${X360PORT_VALIDATION_HEADERS})
target_include_directories(x360port_validation PUBLIC include)
target_compile_options(x360port_validation PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()

    add_executable(x360port_contract_tests tests/contract_tests.cpp)
    target_link_libraries(x360port_contract_tests PRIVATE x360port_validation)
    target_compile_options(x360port_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME contract COMMAND x360port_contract_tests)

    add_test(
        NAME format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror
                ${X360PORT_VALIDATION_SOURCES}
                ${X360PORT_VALIDATION_HEADERS}
                tests/contract_tests.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(
        NAME clang_tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE} -p=${CMAKE_BINARY_DIR}
                --warnings-as-errors=*
                ${X360PORT_VALIDATION_SOURCES}
                tests/contract_tests.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME structure
        COMMAND ${Python3_EXECUTABLE} tools/check_structure.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME structure_selftest
        COMMAND ${Python3_EXECUTABLE} tools/check_structure.py --selftest
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
