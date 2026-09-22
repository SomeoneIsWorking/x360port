#ifndef X360PORT_TESTS_GUARDED_MAIN_HPP
#define X360PORT_TESTS_GUARDED_MAIN_HPP

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace x360port::tests
{

// Runs a test program's body at the process boundary: an exception the body
// does not handle (an allocation failure, say) fails the test by name instead
// of escaping main.
template <typename Body> [[nodiscard]] int GuardedMain(const char* program, Body body) noexcept
{
    try
    {
        return body();
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s: unhandled exception: %s\n", program, error.what());
    }
    catch (...)
    {
        std::fprintf(stderr, "%s: unhandled non-standard exception\n", program);
    }
    return EXIT_FAILURE;
}

} // namespace x360port::tests

#endif
