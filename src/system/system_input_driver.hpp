#ifndef X360PORT_SYSTEM_INPUT_DRIVER_HPP
#define X360PORT_SYSTEM_INPUT_DRIVER_HPP

#include "x360port/system_session.hpp"

#include <memory>

namespace xe::hid
{
class InputDriver;
}

namespace x360port
{

// Adapts the title's controller source to the console's input exports. When a
// set-up host gamepad driver is supplied, its pad is merged with the title's
// (see PadMerger), so either device drives the game at any moment; without
// one, the title's source is the only pad. Xenia's kernel only serializes the
// result into the guest's structures.
[[nodiscard]] std::unique_ptr<xe::hid::InputDriver>
CreateSystemInputDriver(SystemInputSource source, std::unique_ptr<xe::hid::InputDriver> host);

} // namespace x360port

#endif
