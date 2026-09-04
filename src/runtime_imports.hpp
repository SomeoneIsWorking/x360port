#ifndef X360PORT_RUNTIME_IMPORTS_HPP
#define X360PORT_RUNTIME_IMPORTS_HPP

#include "x360port/runtime.hpp"

#include <memory>
#include <span>

namespace xe
{
class Memory;
namespace cpu
{
class ExportResolver;
class RawModule;
} // namespace cpu
} // namespace xe

namespace x360port
{

struct RuntimeImportsCreateResult;

class RuntimeImports final
{
  public:
    RuntimeImports(const RuntimeImports&) = delete;
    RuntimeImports& operator=(const RuntimeImports&) = delete;
    ~RuntimeImports();

    [[nodiscard]] static RuntimeImportsCreateResult
    Create(std::span<const ImportRequirement> requirements,
           std::span<const ImportBinding> bindings);

    [[nodiscard]] RuntimeFailure Attach(xe::cpu::ExportResolver& resolver,
                                        xe::cpu::RawModule& module, xe::Memory& memory);

  private:
    class Impl;

    explicit RuntimeImports(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

struct RuntimeImportsCreateResult
{
    std::unique_ptr<RuntimeImports> imports;
    RuntimeFailure failure;

    [[nodiscard]] explicit operator bool() const noexcept { return imports != nullptr && !failure; }
};

} // namespace x360port

#endif
