cmake_minimum_required(VERSION 3.25)

project(x360port VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/Warnings.cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)

option(X360PORT_VALIDATION_ONLY
       "Build the synthetic validators without the Xenia runtime (diagnostic only)"
       OFF)
set(X360PORT_XENIA_SOURCE_DIR "" CACHE PATH
    "Path to the pinned Xenia source checkout used by the x360port executor")
# dependencies.json is the only authority for the pinned Xenia revision; the
# workflow and tooling read the same file through tools/dependency_pin.py.
file(READ "${CMAKE_CURRENT_LIST_DIR}/dependencies.json" X360PORT_DEPENDENCIES_JSON)
string(JSON X360PORT_XENIA_REVISION GET "${X360PORT_DEPENDENCIES_JSON}" xenia revision)
string(JSON X360PORT_XENIA_REPOSITORY GET "${X360PORT_DEPENDENCIES_JSON}" xenia repository)

set(X360PORT_VALIDATION_SOURCES
    src/digest.cpp
    src/module_validation.cpp
    src/pe_image.cpp
    src/validation.cpp
)
set(X360PORT_VALIDATION_HEADERS
    include/x360port/guest_endian.hpp
    include/x360port/module_contract.hpp
    include/x360port/pe_image.hpp
    include/x360port/validation.hpp
)

# Keep the validators independently buildable for diagnostic use. The runtime
# target below is the only product executor.
add_library(x360port_validation STATIC
    ${X360PORT_VALIDATION_SOURCES}
    ${X360PORT_VALIDATION_HEADERS})
target_include_directories(x360port_validation PUBLIC include)
x360port_enable_warnings(x360port_validation)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(x360port_validation PUBLIC $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>)
endif()

set(X360PORT_RUNTIME_SOURCES
    src/desktop_input.cpp
    src/device_dispatch.cpp
    src/export_names.cpp
    src/import_claims.cpp
    src/executable_invalidation.cpp
    src/guest_execution.cpp
    src/guest_fault_guard.cpp
    src/guest_memory.cpp
    src/guest_thread_context.cpp
    src/guest_virtual_memory.cpp
    src/interpreter_fallback.cpp
    src/override_dispatch.cpp
    src/runtime.cpp
    src/runtime_errors.cpp
    src/runtime_imports.cpp
    src/xam_input.cpp
    src/xenia_backend.cpp
    src/xenia_instance_lease.cpp
    src/xex_helpers.cpp
    src/xex_imports.cpp
    src/xex_inspect.cpp
)
set(X360PORT_RUNTIME_HEADERS
    include/x360port/desktop_input.hpp
    include/x360port/export_names.hpp
    include/x360port/guest_call.hpp
    include/x360port/import_claims.hpp
    include/x360port/runtime.hpp
    include/x360port/runtime_failure.hpp
    include/x360port/xam_input.hpp
    src/device_dispatch.hpp
    src/executable_invalidation.hpp
    src/guest_endian.hpp
    src/guest_memory.hpp
    src/guest_thread_context.hpp
    src/guest_virtual_memory.hpp
    src/interpreter_fallback.hpp
    src/guest_call_frame.hpp
    src/guest_execution.hpp
    src/guest_execution_budget.hpp
    src/override_dispatch.hpp
    src/runtime_imports.hpp
    src/xenia_backend.hpp
    src/xex_helpers.hpp
    src/xex_imports.hpp
    src/xenia_instance_lease.hpp
    include/x360port/xex_inspect.hpp
)
set(X360PORT_SYSTEM_SOURCES
    src/system/desktop_capture_gtk.cpp
    src/system/frame_intervals.cpp
    src/system/pad_merge.cpp
    src/system/system_call_context.cpp
    src/system/system_input_driver.cpp
    src/system/system_session.cpp
    src/system/system_window_gtk.cpp
)
set(X360PORT_SYSTEM_HEADERS
    include/x360port/frame_intervals.hpp
    include/x360port/system_session.hpp
    src/system/desktop_capture_gtk.hpp
    src/system/pad_merge.hpp
    src/system/system_call_context.hpp
    src/system/system_input_driver.hpp
    src/system/system_session_impl.hpp
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
    include(cmake/XeniaRegressionTests.cmake)

    add_library(x360port STATIC
        ${X360PORT_RUNTIME_SOURCES}
        ${X360PORT_RUNTIME_HEADERS})
    add_library(x360port::x360port ALIAS x360port)
    target_include_directories(x360port PUBLIC include)
    target_include_directories(x360port SYSTEM PRIVATE
        "${X360PORT_XENIA_SOURCE_DIR}"
        "${X360PORT_XENIA_SOURCE_DIR}/src")
    x360port_enable_warnings(x360port)
    # Xenia changes the GNU libstdc++ container ABI for its Debug objects. The
    # runtime constructs Xenia-owned types, so its translation unit must use the
    # same layout even though x360port's public Pimpl boundary hides those types.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(x360port PUBLIC $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>)
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
    # The full console: Xenia's kernel, file system, audio, GPU, and input
    # composed around one title. It needs a windowed host for its product
    # path, which exists for GTK on Linux; elsewhere the target is absent and
    # a consumer that requires it fails at configure time by this name.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_library(x360port_system STATIC
            ${X360PORT_SYSTEM_SOURCES}
            ${X360PORT_SYSTEM_HEADERS})
        add_library(x360port::system ALIAS x360port_system)
        target_include_directories(x360port_system SYSTEM PRIVATE
            "${X360PORT_XENIA_SOURCE_DIR}"
            "${X360PORT_XENIA_SOURCE_DIR}/src"
            "${X360PORT_XENIA_SOURCE_DIR}/third_party/Vulkan-Headers/include")
        x360port_enable_warnings(x360port_system)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(X360PORT_GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
        # Only the subsystems x360port's executor does not already link; its
        # own link group resolves their references back into the core. The
        # core comes first: CMake drops a group member's own link dependencies
        # (xenia-ui's X11 and fontconfig) when that member is first reached
        # outside its group, here through xenia-ui-vulkan.
        target_link_libraries(x360port_system PUBLIC
            x360port
            xenia-apu-sdl xenia-apu xenia-gpu-vulkan xenia-ui-vulkan xenia-hid-sdl xenia-hid xenia-vfs
            PkgConfig::X360PORT_GTK3)
        # Xenia's SDL2 interface is ordinary, so its headers would be checked
        # as first-party code; they are a dependency's.
        set_property(TARGET SDL2 PROPERTY SYSTEM TRUE)
    endif()

    add_executable(xex_inspect tools/xex_inspect.cpp)
    target_link_libraries(xex_inspect PRIVATE x360port)
    x360port_enable_warnings(xex_inspect)
    set_target_properties(xex_inspect PROPERTIES OUTPUT_NAME "x360-xex-inspect")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()

    add_library(x360port_warning_probe OBJECT tests/warnings/probe.cpp)
    x360port_enable_warnings(x360port_warning_probe)

    add_executable(x360port_contract_tests tests/contract_tests.cpp)
    target_link_libraries(x360port_contract_tests PRIVATE x360port_validation)
    x360port_enable_warnings(x360port_contract_tests)
    add_test(NAME contract COMMAND x360port_contract_tests)

    if(NOT X360PORT_VALIDATION_ONLY)
        add_executable(x360port_runtime_tests tests/runtime_tests.cpp)
        target_link_libraries(x360port_runtime_tests PRIVATE x360port)
        x360port_enable_warnings(x360port_runtime_tests)
        add_test(NAME runtime COMMAND x360port_runtime_tests)

        add_executable(x360port_import_runtime_tests tests/import_runtime_tests.cpp)
        target_link_libraries(x360port_import_runtime_tests PRIVATE x360port)
        x360port_enable_warnings(x360port_import_runtime_tests)
        add_test(NAME import_runtime COMMAND x360port_import_runtime_tests)

        add_executable(x360port_kernel_memory_tests tests/kernel_memory_tests.cpp)
        target_link_libraries(x360port_kernel_memory_tests PRIVATE x360port)
        x360port_enable_warnings(x360port_kernel_memory_tests)
        add_test(NAME kernel_memory COMMAND x360port_kernel_memory_tests)

        add_executable(x360port_export_name_tests tests/export_name_tests.cpp)
        target_link_libraries(x360port_export_name_tests PRIVATE x360port)
        x360port_enable_warnings(x360port_export_name_tests)
        add_test(NAME export_names COMMAND x360port_export_name_tests)

        add_executable(x360port_desktop_input_tests tests/desktop_input_tests.cpp)
        target_link_libraries(x360port_desktop_input_tests PRIVATE x360port)
        x360port_enable_warnings(x360port_desktop_input_tests)
        add_test(NAME desktop_input COMMAND x360port_desktop_input_tests)

        add_executable(x360port_xex_inspect_tests tests/xex_inspect_tests.cpp)
        target_link_libraries(x360port_xex_inspect_tests PRIVATE x360port)
        x360port_enable_warnings(x360port_xex_inspect_tests)
        add_test(NAME xex_inspect COMMAND x360port_xex_inspect_tests)

        if(TARGET x360port_system)
            add_executable(x360port_system_config_tests tests/system_config_tests.cpp)
            target_link_libraries(x360port_system_config_tests PRIVATE x360port_system)
            x360port_enable_warnings(x360port_system_config_tests)
            add_test(NAME system_config COMMAND x360port_system_config_tests)

            add_executable(x360port_frame_interval_tests tests/frame_interval_tests.cpp)
            target_link_libraries(x360port_frame_interval_tests PRIVATE x360port_system)
            x360port_enable_warnings(x360port_frame_interval_tests)
            add_test(NAME frame_intervals COMMAND x360port_frame_interval_tests)

            add_executable(x360port_pad_merge_tests tests/pad_merge_tests.cpp)
            target_include_directories(x360port_pad_merge_tests PRIVATE src/system)
            target_link_libraries(x360port_pad_merge_tests PRIVATE x360port_system)
            x360port_enable_warnings(x360port_pad_merge_tests)
            add_test(NAME pad_merge COMMAND x360port_pad_merge_tests)
        endif()
    endif()

    add_test(
        NAME format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror
                ${X360PORT_VALIDATION_SOURCES}
                ${X360PORT_VALIDATION_HEADERS}
                ${X360PORT_RUNTIME_SOURCES}
                ${X360PORT_RUNTIME_HEADERS}
                ${X360PORT_SYSTEM_SOURCES}
                ${X360PORT_SYSTEM_HEADERS}
                tests/contract_tests.cpp
                tests/desktop_input_tests.cpp
                tests/export_name_tests.cpp
                tests/frame_interval_tests.cpp
                tests/import_runtime_tests.cpp
                tests/kernel_memory_tests.cpp
                tests/pad_merge_tests.cpp
                tests/system_config_tests.cpp
                tests/runtime_tests.cpp
                tests/xex_inspect_tests.cpp
                tests/warnings/probe.cpp
                tools/xex_inspect.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    # AppleClang finds the macOS SDK itself, so CMake 4 leaves -isysroot out
    # of the compile database; another clang driver such as clang-tidy would
    # then find no standard headers. Name the SDK xcrun selects.
    set(X360PORT_CLANG_TIDY_SDK_ARGS)
    if(APPLE)
        execute_process(
            COMMAND xcrun --show-sdk-path
            RESULT_VARIABLE X360PORT_SDK_RESULT
            OUTPUT_VARIABLE X360PORT_SDK_PATH
            ERROR_VARIABLE X360PORT_SDK_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT X360PORT_SDK_RESULT EQUAL 0 OR NOT IS_DIRECTORY "${X360PORT_SDK_PATH}")
            message(FATAL_ERROR
                "clang-tidy needs the macOS SDK; xcrun --show-sdk-path gave "
                "'${X360PORT_SDK_PATH}' (${X360PORT_SDK_RESULT}): ${X360PORT_SDK_ERROR}")
        endif()
        set(X360PORT_CLANG_TIDY_SDK_ARGS "--extra-arg=-isysroot${X360PORT_SDK_PATH}")
    endif()
    add_test(
        NAME clang_tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE} -p=${CMAKE_BINARY_DIR}
                --warnings-as-errors=* ${X360PORT_CLANG_TIDY_SDK_ARGS}
                ${X360PORT_VALIDATION_SOURCES}
                tests/contract_tests.cpp
                tests/warnings/probe.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT X360PORT_VALIDATION_ONLY)
        add_test(
            NAME runtime_clang_tidy
            COMMAND ${CLANG_TIDY_EXECUTABLE} -p=${CMAKE_BINARY_DIR}
                    --warnings-as-errors=* ${X360PORT_CLANG_TIDY_SDK_ARGS}
                    ${X360PORT_RUNTIME_SOURCES}
                    tests/desktop_input_tests.cpp
                    tests/export_name_tests.cpp
                    tests/import_runtime_tests.cpp
                    tests/kernel_memory_tests.cpp
                    tests/runtime_tests.cpp
                    tests/xex_inspect_tests.cpp
                    tools/xex_inspect.cpp
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        if(TARGET x360port_system)
            add_test(
                NAME system_clang_tidy
                COMMAND ${CLANG_TIDY_EXECUTABLE} -p=${CMAKE_BINARY_DIR}
                        --warnings-as-errors=* ${X360PORT_CLANG_TIDY_SDK_ARGS}
                        ${X360PORT_SYSTEM_SOURCES}
                        tests/frame_interval_tests.cpp
                        tests/pad_merge_tests.cpp
                        tests/system_config_tests.cpp
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif()
    endif()
    add_test(NAME structure
        COMMAND ${Python3_EXECUTABLE} tools/check_structure.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME structure_selftest
        COMMAND ${Python3_EXECUTABLE} tools/check_structure.py --selftest
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
