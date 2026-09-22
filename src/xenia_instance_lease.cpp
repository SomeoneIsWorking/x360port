#include "xenia_instance_lease.hpp"

#include <atomic>

namespace x360port
{
namespace
{

std::atomic<bool> g_instance_active{false};

} // namespace

XeniaInstanceLease::~XeniaInstanceLease()
{
    if (held_)
    {
        g_instance_active.store(false, std::memory_order_release);
    }
}

bool XeniaInstanceLease::Acquire() noexcept
{
    if (held_)
    {
        return true;
    }
    bool expected = false;
    held_ = g_instance_active.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
    return held_;
}

} // namespace x360port
