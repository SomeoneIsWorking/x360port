#include "guest_memory_load.hpp"

#include <limits>
#include <sys/mman.h>
#include <unistd.h>

namespace xenon_host
{
namespace
{

[[nodiscard]] std::byte* ReserveWindow(std::uint64_t size) noexcept
{
    if (size > std::numeric_limits<std::size_t>::max())
    {
        return nullptr;
    }
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_NORESERVE
    flags |= MAP_NORESERVE;
#endif
    void* mapping = mmap(nullptr, static_cast<std::size_t>(size), PROT_NONE, flags, -1, 0);
    return mapping == MAP_FAILED ? nullptr : static_cast<std::byte*>(mapping);
}

[[nodiscard]] bool CommitWindow(std::byte* window, GuestMemoryRange range) noexcept
{
    const long page_size_value = sysconf(_SC_PAGESIZE);
    if (window == nullptr || page_size_value <= 0 || range.byte_count == 0)
    {
        return false;
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(page_size_value);
    const std::uint64_t begin = (static_cast<std::uint64_t>(range.address) / page_size) * page_size;
    const std::uint64_t range_end = static_cast<std::uint64_t>(range.address) + range.byte_count;
    const std::uint64_t end = ((range_end + page_size - 1U) / page_size) * page_size;
    if (range_end > GuestMemory::WindowSize || end > GuestMemory::WindowSize)
    {
        return false;
    }
    return mprotect(window + begin, static_cast<std::size_t>(end - begin),
                    PROT_READ | PROT_WRITE) == 0;
}

void ReleaseWindow(std::byte* window, std::uint64_t size) noexcept
{
    if (window != nullptr && size <= std::numeric_limits<std::size_t>::max())
    {
        static_cast<void>(munmap(window, static_cast<std::size_t>(size)));
    }
}

} // namespace

const GuestVirtualMemoryOps& GuestMemoryLoader::PlatformOperations() noexcept
{
    static constexpr GuestVirtualMemoryOps Operations = {
        .reserve = ReserveWindow,
        .commit = CommitWindow,
        .release = ReleaseWindow,
    };
    return Operations;
}

} // namespace xenon_host
