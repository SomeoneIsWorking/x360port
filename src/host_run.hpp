#ifndef XENON_HOST_HOST_RUN_HPP
#define XENON_HOST_HOST_RUN_HPP

#include "guest_memory_load.hpp"
#include "xenon_host/host.hpp"

namespace xenon_host
{

class HostRunner final
{
  public:
    [[nodiscard]] static RunResult Run(TitleAdapter& adapter, RunRequest request,
                                       const GuestVirtualMemoryOps& operations) noexcept;
};

} // namespace xenon_host

#endif
