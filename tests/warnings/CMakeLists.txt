cmake_minimum_required(VERSION 3.25)
project(x360port_warning_contract LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/Warnings.cmake")

function(probe_driver name compiler frontend msvc)
    set(CMAKE_CXX_COMPILER_ID "${compiler}")
    set(CMAKE_CXX_COMPILER_FRONTEND_VARIANT "${frontend}")
    set(MSVC "${msvc}")
    add_library(${name} OBJECT probe.cpp)
    x360port_enable_warnings(${name})
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.flags"
        CONTENT "$<JOIN:$<TARGET_PROPERTY:${name},COMPILE_OPTIONS>,\n>\n")
endfunction()

probe_driver(gnu Clang GNU FALSE)
probe_driver(clang_cl Clang MSVC TRUE)
probe_driver(msvc MSVC MSVC TRUE)
