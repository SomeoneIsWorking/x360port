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

// Adapts the title's controller arbitration to the console's input exports.
// The title stays the single owner of which host source a pad reports; Xenia's
// kernel only serializes the snapshot into the guest's structures.
[[nodiscard]] std::unique_ptr<xe::hid::InputDriver>
CreateSystemInputDriver(SystemInputSource source);

} // namespace x360port

#endif
