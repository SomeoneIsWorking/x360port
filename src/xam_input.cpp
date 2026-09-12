#include "x360port/xam_input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace x360port
{
namespace
{

constexpr std::size_t kInputStateBytes = 16U;

void Store16(std::array<std::byte, kInputStateBytes>& bytes, std::size_t offset,
             std::uint16_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 1U] = static_cast<std::byte>(value);
}

void Store32(std::array<std::byte, kInputStateBytes>& bytes, std::size_t offset,
             std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 24U);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 3U] = static_cast<std::byte>(value);
}

[[nodiscard]] std::array<std::byte, kInputStateBytes>
EncodeState(const XamPadSnapshot& snapshot) noexcept
{
    std::array<std::byte, kInputStateBytes> bytes{};
    if (!snapshot.connected)
    {
        return bytes;
    }
    Store32(bytes, 0U, snapshot.packet_number);
    Store16(bytes, 4U, snapshot.buttons);
    bytes[6] = static_cast<std::byte>(snapshot.left_trigger);
    bytes[7] = static_cast<std::byte>(snapshot.right_trigger);
    Store16(bytes, 8U, static_cast<std::uint16_t>(snapshot.thumb_lx));
    Store16(bytes, 10U, static_cast<std::uint16_t>(snapshot.thumb_ly));
    Store16(bytes, 12U, static_cast<std::uint16_t>(snapshot.thumb_rx));
    Store16(bytes, 14U, static_cast<std::uint16_t>(snapshot.thumb_ry));
    return bytes;
}

} // namespace

void XamInputService::Bind(const ImportRequirement& requirement, ImportBinding& binding) noexcept
{
    if (requirement.kind != ImportKind::Function || requirement.library != "xam.xex" ||
        requirement.ordinal != kXamInputGetStateOrdinal)
    {
        return;
    }
    binding.function_handler = GetState;
    binding.function_context = this;
}

void XamInputService::GetState(GuestImportContext& call, void* service) noexcept
{
    auto& input = *static_cast<XamInputService*>(service);
    if (input.reader_ == nullptr)
    {
        call.refuse(ImportRefusalReason::HostUnavailable);
        return;
    }

    const XamInputRequest request{static_cast<std::uint32_t>(call.argument(0)),
                                  static_cast<std::uint32_t>(call.argument(1))};
    const XamPadSnapshot snapshot = input.reader_(request, input.reader_context_);
    const GuestAddress state_address = static_cast<GuestAddress>(call.argument(2));
    if (state_address != 0U)
    {
        const auto bytes = EncodeState(snapshot);
        if (!call.write_memory(state_address, bytes))
        {
            call.refuse(ImportRefusalReason::InvalidGuestMemory);
            return;
        }
    }
    call.set_return_value(snapshot.connected ? 0U : kXamInputDeviceNotConnected);
}

} // namespace x360port
