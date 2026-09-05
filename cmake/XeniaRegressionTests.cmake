# Consume the maintained runtime and its explicitly bounded portability suite.
# Xenia owns the source roster, platform entry point, ABI, and Catch discovery.
set(XENIA_BUILD_PORTABILITY_TESTS "${PROJECT_IS_TOP_LEVEL}" CACHE BOOL "" FORCE)
add_subdirectory("${X360PORT_XENIA_SOURCE_DIR}" xenia EXCLUDE_FROM_ALL)
if(PROJECT_IS_TOP_LEVEL)
    if(NOT TARGET xenia-portability-tests)
        message(FATAL_ERROR
            "x360port: the pinned Xenia checkout must provide xenia-portability-tests")
    endif()
    # The suite belongs to the framework's gate, not its player-facing consumers.
    add_custom_target(x360port_xenia_regressions ALL DEPENDS xenia-portability-tests)
endif()
