#include "x360port/xam_input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace x360port
{
namespace
{

constexpr std::size_t kInputStateBytes = 16U;
constexpr std::size_t kInputCapabilitiesBytes = 20U;

template <std::size_t Size>
void Store16(std::array<std::byte, Size>& bytes, std::size_t offset, std::uint16_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 1U] = static_cast<std::byte>(value);
}

template <std::size_t Size>
void Store32(std::array<std::byte, Size>& bytes, std::size_t offset, std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 24U);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 3U] = static_cast<std::byte>(value);
}

template <std::size_t Size>
void StoreGamepad(std::array<std::byte, Size>& bytes, std::size_t offset,
                  const XamGamepad& gamepad) noexcept
{
    Store16(bytes, offset, gamepad.buttons);
    bytes[offset + 2U] = static_cast<std::byte>(gamepad.left_trigger);
    bytes[offset + 3U] = static_cast<std::byte>(gamepad.right_trigger);
    Store16(bytes, offset + 4U, static_cast<std::uint16_t>(gamepad.thumb_lx));
    Store16(bytes, offset + 6U, static_cast<std::uint16_t>(gamepad.thumb_ly));
    Store16(bytes, offset + 8U, static_cast<std::uint16_t>(gamepad.thumb_rx));
    Store16(bytes, offset + 10U, static_cast<std::uint16_t>(gamepad.thumb_ry));
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
    StoreGamepad(bytes, 4U, snapshot.gamepad);
    return bytes;
}

[[nodiscard]] std::array<std::byte, kInputCapabilitiesBytes>
EncodeCapabilities(const XamPadCapabilities& capabilities) noexcept
{
    std::array<std::byte, kInputCapabilitiesBytes> bytes{};
    if (!capabilities.connected)
    {
        return bytes;
    }
    bytes[0] = static_cast<std::byte>(capabilities.type);
    bytes[1] = static_cast<std::byte>(capabilities.sub_type);
    Store16(bytes, 2U, capabilities.flags);
    StoreGamepad(bytes, 4U, capabilities.supported_gamepad);
    Store16(bytes, 16U, capabilities.left_motor_speed);
    Store16(bytes, 18U, capabilities.right_motor_speed);
    return bytes;
}

} // namespace

void XamInputService::Bind(const ImportRequirement& requirement, ImportBinding& binding) noexcept
{
    if (requirement.kind != ImportKind::Function || requirement.library != "xam.xex")
    {
        return;
    }
    if (requirement.ordinal == kXamInputGetStateOrdinal)
    {
        binding.function_handler = GetState;
        binding.function_context = this;
    }
    if (requirement.ordinal == kXamInputGetCapabilitiesOrdinal)
    {
        binding.function_handler = GetCapabilities;
        binding.function_context = this;
    }
}

void XamInputService::GetState(GuestImportContext& call, void* service) noexcept
{
    auto& input = *static_cast<XamInputService*>(service);
    if (input.state_reader_ == nullptr)
    {
        call.refuse(ImportRefusalReason::HostUnavailable);
        return;
    }

    const XamInputRequest request{static_cast<std::uint32_t>(call.argument(0)),
                                  static_cast<std::uint32_t>(call.argument(1))};
    const XamPadSnapshot snapshot = input.state_reader_(request, input.reader_context_);
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

void XamInputService::GetCapabilities(GuestImportContext& call, void* service) noexcept
{
    auto& input = *static_cast<XamInputService*>(service);
    if (input.capabilities_reader_ == nullptr)
    {
        call.refuse(ImportRefusalReason::HostUnavailable);
        return;
    }
    const GuestAddress capabilities_address = static_cast<GuestAddress>(call.argument(2));
    if (capabilities_address == 0U)
    {
        call.set_return_value(kXamInputBadArguments);
        return;
    }
    const XamInputRequest request{static_cast<std::uint32_t>(call.argument(0)),
                                  static_cast<std::uint32_t>(call.argument(1))};
    const XamPadCapabilities capabilities =
        input.capabilities_reader_(request, input.reader_context_);
    const auto bytes = EncodeCapabilities(capabilities);
    if (!call.write_memory(capabilities_address, bytes))
    {
        call.refuse(ImportRefusalReason::InvalidGuestMemory);
        return;
    }
    call.set_return_value(capabilities.connected ? 0U : kXamInputDeviceNotConnected);
}

} // namespace x360port
