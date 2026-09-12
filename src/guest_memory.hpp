#ifndef X360PORT_GUEST_MEMORY_HPP
#define X360PORT_GUEST_MEMORY_HPP

#include "x360port/runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>

namespace xe
{
class Memory;
}

namespace x360port
{

class GuestMemory final
{
  public:
    GuestMemory() = default;
    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    ~GuestMemory();

    void Initialize(xe::Memory& memory) noexcept;
    void Reset() noexcept;

    [[nodiscard]] GuestMemoryAllocationResult Allocate(std::uint32_t size);
    [[nodiscard]] RuntimeFailure Read(GuestAddress address, std::span<std::byte> bytes) const;
    [[nodiscard]] RuntimeFailure Write(GuestAddress address,
                                       std::span<const std::byte> bytes) const;
    [[nodiscard]] RuntimeFailure Release(GuestMemoryAllocation allocation);

  private:
    [[nodiscard]] bool Contains(GuestAddress address, std::size_t size) const noexcept;

    xe::Memory* memory_ = nullptr;
    std::map<GuestAddress, std::uint32_t> allocations_;
};

} // namespace x360port

#endif
