#ifndef X360PORT_XENIA_BACKEND_HPP
#define X360PORT_XENIA_BACKEND_HPP

#include <memory>

namespace xe::cpu::backend
{
class Backend;
}

namespace x360port
{

[[nodiscard]] std::unique_ptr<xe::cpu::backend::Backend> CreateXeniaHostBackend();

} // namespace x360port

#endif
