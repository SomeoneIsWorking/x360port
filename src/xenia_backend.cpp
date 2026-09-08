#include "xenia_backend.hpp"

#include "xenia/base/platform.h"
#include "xenia/cpu/backend/backend.h"
#if XE_ARCH_AMD64
#include "xenia/cpu/backend/x64/x64_backend.h"
#elif XE_ARCH_ARM64
#include "xenia/cpu/backend/a64/a64_backend.h"
#else
#error "x360port requires a Xenia x64 or A64 dynarec backend"
#endif

namespace x360port
{

std::unique_ptr<xe::cpu::backend::Backend> CreateXeniaHostBackend()
{
#if XE_ARCH_AMD64
    return std::make_unique<xe::cpu::backend::x64::X64Backend>();
#elif XE_ARCH_ARM64
    return std::make_unique<xe::cpu::backend::a64::A64Backend>();
#endif
}

} // namespace x360port
