function(x360port_enable_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # clang-cl treats bare -Wall as MSVC /Wall, which includes diagnostics
        # about compatibility with obsolete C++ standards. Select Clang's groups
        # explicitly while retaining warnings-as-errors.
        target_compile_options(${target} PRIVATE
            /clang:-Wall /clang:-Wextra /clang:-Wpedantic /WX)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
