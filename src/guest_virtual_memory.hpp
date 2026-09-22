#ifndef X360PORT_GUEST_VIRTUAL_MEMORY_HPP
#define X360PORT_GUEST_VIRTUAL_MEMORY_HPP

#include "x360port/import_claims.hpp"

#include <array>
#include <cstdint>

namespace xe
{
class Memory;
}

namespace x360port
{

// Implements the kernel's virtual-memory exports over the same Xenia heaps the
// embedded Processor already uses. It deliberately owns no allocator state: a
// second allocator beside Memory would disagree with the first about which
// guest pages are committed, and the address-to-heap lookup that decides which
// owner a guest pointer belongs to lives there.
class GuestVirtualMemory final
{
  public:
    explicit GuestVirtualMemory(xe::Memory& memory) noexcept : memory_(&memory) {}

    GuestVirtualMemory(const GuestVirtualMemory&) = delete;
    GuestVirtualMemory& operator=(const GuestVirtualMemory&) = delete;
    GuestVirtualMemory(GuestVirtualMemory&&) = delete;
    GuestVirtualMemory& operator=(GuestVirtualMemory&&) = delete;

    // The exports this service implements. An export the guest calls that is
    // absent here keeps the caller's refusal rather than a guessed answer.
    [[nodiscard]] std::array<ImportClaim, 3> Claims() noexcept;

  private:
    static void Allocate(GuestImportContext& call, void* service) noexcept;
    static void Free(GuestImportContext& call, void* service) noexcept;
    static void Query(GuestImportContext& call, void* service) noexcept;

    xe::Memory* memory_;
};

} // namespace x360port

#endif // X360PORT_GUEST_VIRTUAL_MEMORY_HPP
