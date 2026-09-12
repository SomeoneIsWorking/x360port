#ifndef X360PORT_XAM_INPUT_HPP
#define X360PORT_XAM_INPUT_HPP

#include "x360port/validation.hpp"

#include <cstdint>

namespace x360port
{

inline constexpr std::uint32_t kXamInputGetCapabilitiesOrdinal = 400U;
inline constexpr std::uint32_t kXamInputGetStateOrdinal = 401U;
inline constexpr std::uint32_t kXamInputDeviceNotConnected = 0x48FU;
inline constexpr std::uint32_t kXamInputBadArguments = 0xA0U;

struct XamGamepad
{
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t thumb_lx = 0;
    std::int16_t thumb_ly = 0;
    std::int16_t thumb_rx = 0;
    std::int16_t thumb_ry = 0;
};

struct XamPadSnapshot
{
    bool connected = false;
    std::uint32_t packet_number = 0;
    XamGamepad gamepad;
};

struct XamPadCapabilities
{
    bool connected = false;
    std::uint8_t type = 0;
    std::uint8_t sub_type = 0;
    std::uint16_t flags = 0;
    XamGamepad supported_gamepad;
    std::uint16_t left_motor_speed = 0;
    std::uint16_t right_motor_speed = 0;
};

struct XamInputRequest
{
    std::uint32_t user_index = 0;
    std::uint32_t flags = 0;
};

using XamPadReader = XamPadSnapshot (*)(XamInputRequest request, void* context) noexcept;
using XamCapabilitiesReader = XamPadCapabilities (*)(XamInputRequest request,
                                                     void* context) noexcept;

class XamInputService final
{
  public:
    XamInputService(XamPadReader state_reader, XamCapabilitiesReader capabilities_reader,
                    void* reader_context) noexcept
        : state_reader_(state_reader), capabilities_reader_(capabilities_reader),
          reader_context_(reader_context)
    {
    }

    XamInputService(const XamInputService&) = delete;
    XamInputService& operator=(const XamInputService&) = delete;
    XamInputService(XamInputService&&) = delete;
    XamInputService& operator=(XamInputService&&) = delete;

    // Bind only the platform controller state/capabilities exports. The caller owns this
    // service for as long as the runtime may dispatch its callback.
    void Bind(const ImportRequirement& requirement, ImportBinding& binding) noexcept;

  private:
    static void GetState(GuestImportContext& call, void* service) noexcept;
    static void GetCapabilities(GuestImportContext& call, void* service) noexcept;

    XamPadReader state_reader_;
    XamCapabilitiesReader capabilities_reader_;
    void* reader_context_;
};

} // namespace x360port

#endif
