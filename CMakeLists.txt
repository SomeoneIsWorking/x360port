cmake_minimum_required(VERSION 3.25)

project(x360port VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)

option(X360PORT_VALIDATION_ONLY
       "Build the synthetic validators without the Xenia runtime (diagnostic only)"
       OFF)
set(X360PORT_XENIA_SOURCE_DIR "" CACHE PATH
    "Path to the pinned Xenia source checkout used by the x360port executor")
set(X360PORT_XENIA_REVISION "657d521af176e07a1290524310855c6eb966970c")

set(X360PORT_VALIDATION_SOURCES
    src/digest.cpp
    src/module_validation.cpp
    src/validation.cpp
)
set(X360PORT_VALIDATION_HEADERS
    include/x360port/module_contract.hpp
    include/x360port/validation.hpp
)

# Keep the validators independently buildable for diagnostic use. The runtime
# target below is the only product executor.
add_library(x360port_validation STATIC
    ${X360PORT_VALIDATION_SOURCES}
    ${X360PORT_VALIDATION_HEADERS})
target_include_directories(x360port_validation PUBLIC include)
target_compile_options(x360port_validation PRIVATE -Wall -Wextra -Wpedantic -Werror)

set(X360PORT_RUNTIME_SOURCES
    src/runtime.cpp
    src/runtime_imports.cpp
)
set(X360PORT_RUNTIME_HEADERS
    include/x360port/runtime.hpp
    src/runtime_imports.hpp
)

if(NOT X360PORT_VALIDATION_ONLY)
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR "x360port: the Xenia executor must be built with Ninja")
    endif()
    if(X360PORT_XENIA_SOURCE_DIR STREQUAL "")
        message(FATAL_ERROR
            "x360port: X360PORT_XENIA_SOURCE_DIR must name the pinned Xenia checkout. "
            "The runtime never substitutes a validator, interpreter, or generated-code backend.")
    endif()
    if(NOT EXISTS "${X360PORT_XENIA_SOURCE_DIR}/src/xenia/cpu/processor.cc")
        message(FATAL_ERROR
            "x360port: X360PORT_XENIA_SOURCE_DIR is not a complete Xenia source checkout: "
            "${X360PORT_XENIA_SOURCE_DIR}")
    endif()
    execute_process(
        COMMAND git -C "${X360PORT_XENIA_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE X360PORT_XENIA_ACTUAL_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE X360PORT_XENIA_REVISION_RESULT)
    if(NOT X360PORT_XENIA_REVISION_RESULT EQUAL 0 OR
       NOT X360PORT_XENIA_ACTUAL_REVISION STREQUAL X360PORT_XENIA_REVISION)
        message(FATAL_ERROR
            "x360port: Xenia revision mismatch; expected ${X360PORT_XENIA_REVISION}, "
            "observed ${X360PORT_XENIA_ACTUAL_REVISION}")
    endif()
    string(SUBSTRING "${X360PORT_XENIA_ACTUAL_REVISION}" 0 7
           X360PORT_XENIA_SHORT_REVISION)
    string(CONCAT X360PORT_XENIA_VERSION_HEADER
        "#define XE_BUILD_BRANCH \"x360port-pinned\"\n"
        "#define XE_BUILD_COMMIT \"${X360PORT_XENIA_ACTUAL_REVISION}\"\n"
        "#define XE_BUILD_COMMIT_SHORT \"${X360PORT_XENIA_SHORT_REVISION}\"\n"
        "#define XE_BUILD_DATE \"\"\n")
    file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/version.h"
        CONTENT "${X360PORT_XENIA_VERSION_HEADER}" @ONLY)

    set(XENIA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(XENIA_BUILD_MISC OFF CACHE BOOL "" FORCE)
    add_subdirectory("${X360PORT_XENIA_SOURCE_DIR}" xenia EXCLUDE_FROM_ALL)

    add_library(x360port STATIC
        ${X360PORT_RUNTIME_SOURCES}
        ${X360PORT_RUNTIME_HEADERS})
    add_library(x360port::x360port ALIAS x360port)
    target_include_directories(x360port PUBLIC include)
    target_include_directories(x360port SYSTEM PRIVATE
        "${X360PORT_XENIA_SOURCE_DIR}"
        "${X360PORT_XENIA_SOURCE_DIR}/src")
    target_compile_options(x360port PRIVATE -Wall -Wextra -Wpedantic -Werror)
    # Xenia changes the GNU libstdc++ container ABI for its Debug objects. The
    # runtime constructs Xenia-owned types, so its translation unit must use the
    # same layout even though x360port's public Pimpl boundary hides those types.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(x360port PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>)
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        set(X360PORT_XENIA_BACKEND_TARGET xenia-cpu-backend-x64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|ARM64|arm64)$")
        set(X360PORT_XENIA_BACKEND_TARGET xenia-cpu-backend-a64)
    else()
        message(FATAL_ERROR
            "x360port: Xenia has no dynarec backend for ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set(X360PORT_XENIA_LIBRARIES
        xenia-core
        xenia-kernel
        xenia-cpu
        ${X360PORT_XENIA_BACKEND_TARGET}
        xenia-gpu
        xenia-patcher
        xenia-ui
        imgui)
    if(CMAKE_CXX_LINK_GROUP_USING_RESCAN_SUPPORTED OR
       CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED)
        target_link_libraries(x360port PUBLIC
            x360port_validation
            "$<LINK_GROUP:RESCAN,${X360PORT_XENIA_LIBRARIES}>")
    else()
        target_link_libraries(x360port PUBLIC
            x360port_validation
            ${X360PORT_XENIA_LIBRARIES})
    endif()
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()

    add_executable(x360port_contract_tests tests/contract_tests.cpp)
    target_link_libraries(x360port_contract_tests PRIVATE x360port_validation)
    target_compile_options(x360port_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME contract COMMAND x360port_contract_tests)

    if(NOT X360PORT_VALIDATION_ONLY)
        add_executable(x360port_runtime_tests tests/runtime_tests.cpp)
        target_link_libraries(x360port_runtime_tests PRIVATE x360port)
        target_compile_options(x360port_runtime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME runtime COMMAND x360port_runtime_tests)
    endif()

    add_test(
        NAME format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror
                ${X360PORT_VALIDATION_SOURCES}
                ${X360PORT_VALIDATION_HEADERS}
                ${X360PORT_RUNTIME_SOURCES}
                ${X360PORT_RUNTIME_HEADERS}
                tests/contract_tests.cpp
                tests/runtime_tests.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(
        NAME clang_tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE} -p=${CMAKE_BINARY_DIR}
                --warnings-as-errors=*
                ${X360PORT_VALIDATION_SOURCES}
                tests/contract_tests.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT X360PORT_VALIDATION_ONLY)
        add_test(
            NAME runtime_clang_tidy
            COMMAND ${CLANG_TIDY_EXECUTABLE} -p=${CMAKE_BINARY_DIR}
                    --warnings-as-errors=*
                    ${X360PORT_RUNTIME_SOURCES}
                    tests/runtime_tests.cpp
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    add_test(NAME structure
        COMMAND ${Python3_EXECUTABLE} tools/check_structure.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME structure_selftest
        COMMAND ${Python3_EXECUTABLE} tools/check_structure.py --selftest
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
