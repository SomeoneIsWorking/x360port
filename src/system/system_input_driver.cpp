#include "system_input_driver.hpp"

#include "pad_merge.hpp"

#include <utility>

#include "xenia/hid/input_driver.h"

namespace x360port
{
namespace
{

// Xenia's status macros expand to casts through these unqualified names.
using xe::X_RESULT;
using xe::X_STATUS;

[[nodiscard]] XamGamepad LoadGamepad(const xe::hid::X_INPUT_GAMEPAD& source) noexcept
{
    return {.buttons = source.buttons,
            .left_trigger = source.left_trigger,
            .right_trigger = source.right_trigger,
            .thumb_lx = source.thumb_lx,
            .thumb_ly = source.thumb_ly,
            .thumb_rx = source.thumb_rx,
            .thumb_ry = source.thumb_ry};
}

void StoreGamepad(const XamGamepad& source, xe::hid::X_INPUT_GAMEPAD& target) noexcept
{
    target.buttons = source.buttons;
    target.left_trigger = source.left_trigger;
    target.right_trigger = source.right_trigger;
    target.thumb_lx = source.thumb_lx;
    target.thumb_ly = source.thumb_ly;
    target.thumb_rx = source.thumb_rx;
    target.thumb_ry = source.thumb_ry;
}

class SystemInputDriver final : public xe::hid::InputDriver
{
  public:
    SystemInputDriver(SystemInputSource source, std::unique_ptr<xe::hid::InputDriver> host) noexcept
        : InputDriver(nullptr, 0), source_(source), host_(std::move(host))
    {
    }

    X_STATUS Setup() override { return X_STATUS_SUCCESS; }

    X_RESULT GetCapabilities(std::uint32_t user_index, std::uint32_t flags,
                             xe::hid::X_INPUT_CAPABILITIES* out_caps) override
    {
        const XamPadCapabilities capabilities =
            source_.capabilities(XamInputRequest{user_index, flags}, source_.context);
        if (!capabilities.connected)
        {
            // Without the title's pad, a host gamepad describes itself.
            return host_ == nullptr ? X_ERROR_DEVICE_NOT_CONNECTED
                                    : host_->GetCapabilities(user_index, flags, out_caps);
        }
        out_caps->type = capabilities.type;
        out_caps->sub_type = capabilities.sub_type;
        out_caps->flags = capabilities.flags;
        StoreGamepad(capabilities.supported_gamepad, out_caps->gamepad);
        out_caps->vibration.left_motor_speed = capabilities.left_motor_speed;
        out_caps->vibration.right_motor_speed = capabilities.right_motor_speed;
        return X_ERROR_SUCCESS;
    }

    X_RESULT GetState(std::uint32_t user_index, xe::hid::X_INPUT_STATE* out_state) override
    {
        const XamPadSnapshot merged = merger_.Merge(
            source_.state(XamInputRequest{user_index, 0}, source_.context), HostState(user_index));
        if (!merged.connected)
        {
            return X_ERROR_DEVICE_NOT_CONNECTED;
        }
        out_state->packet_number = merged.packet_number;
        StoreGamepad(merged.gamepad, out_state->gamepad);
        return X_ERROR_SUCCESS;
    }

    X_RESULT SetState(std::uint32_t user_index, xe::hid::X_INPUT_VIBRATION* vibration) override
    {
        // A host gamepad takes the rumble it can play. Otherwise a connected
        // pad accepts the request as the console does; an absent one reports
        // that it is absent.
        if (host_ != nullptr && host_->SetState(user_index, vibration) == X_ERROR_SUCCESS)
        {
            return X_ERROR_SUCCESS;
        }
        const XamPadSnapshot snapshot =
            source_.state(XamInputRequest{user_index, 0}, source_.context);
        return snapshot.connected ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
    }

    X_RESULT GetKeystroke(std::uint32_t, std::uint32_t, xe::hid::X_INPUT_KEYSTROKE*) override
    {
        // Keystrokes are derived from pad state by XAM's own keystroke owner
        // only when a driver reports them; this source reports none.
        return X_ERROR_EMPTY;
    }

    [[nodiscard]] xe::hid::InputType GetInputType() const override
    {
        return xe::hid::InputType::Controller;
    }

  private:
    [[nodiscard]] XamPadSnapshot HostState(std::uint32_t user_index)
    {
        if (host_ == nullptr)
        {
            return {};
        }
        xe::hid::X_INPUT_STATE state{};
        if (host_->GetState(user_index, &state) != X_ERROR_SUCCESS)
        {
            return {};
        }
        return {.connected = true,
                .packet_number = state.packet_number,
                .gamepad = LoadGamepad(state.gamepad)};
    }

    SystemInputSource source_;
    std::unique_ptr<xe::hid::InputDriver> host_;
    PadMerger merger_;
};

} // namespace

std::unique_ptr<xe::hid::InputDriver>
CreateSystemInputDriver(SystemInputSource source, std::unique_ptr<xe::hid::InputDriver> host)
{
    return std::make_unique<SystemInputDriver>(source, std::move(host));
}

} // namespace x360port
