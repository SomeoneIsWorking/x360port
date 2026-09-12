#ifndef X360PORT_XAM_INPUT_HPP
#define X360PORT_XAM_INPUT_HPP

#include "x360port/validation.hpp"

#include <cstdint>

namespace x360port
{

inline constexpr std::uint32_t kXamInputGetStateOrdinal = 401U;
inline constexpr std::uint32_t kXamInputDeviceNotConnected = 0x48FU;

struct XamPadSnapshot
{
    bool connected = false;
    std::uint32_t packet_number = 0;
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t thumb_lx = 0;
    std::int16_t thumb_ly = 0;
    std::int16_t thumb_rx = 0;
    std::int16_t thumb_ry = 0;
};

struct XamInputRequest
{
    std::uint32_t user_index = 0;
    std::uint32_t flags = 0;
};

using XamPadReader = XamPadSnapshot (*)(XamInputRequest request, void* context) noexcept;

class XamInputService final
{
  public:
    XamInputService(XamPadReader reader, void* reader_context) noexcept
        : reader_(reader), reader_context_(reader_context)
    {
    }

    XamInputService(const XamInputService&) = delete;
    XamInputService& operator=(const XamInputService&) = delete;
    XamInputService(XamInputService&&) = delete;
    XamInputService& operator=(XamInputService&&) = delete;

    // Bind only the platform XamInputGetState export. The caller owns this
    // service for as long as the runtime may dispatch its callback.
    void Bind(const ImportRequirement& requirement, ImportBinding& binding) noexcept;

  private:
    static void GetState(GuestImportContext& call, void* service) noexcept;

    XamPadReader reader_;
    void* reader_context_;
};

} // namespace x360port

#endif
