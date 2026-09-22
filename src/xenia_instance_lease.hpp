#ifndef X360PORT_XENIA_INSTANCE_LEASE_HPP
#define X360PORT_XENIA_INSTANCE_LEASE_HPP

namespace x360port
{

// Xenia maps the guest address space at a process-wide fixed host address, so
// at most one owner of an xe::Memory may exist at a time. Every x360port owner
// that constructs one holds this lease for exactly that lifetime.
class XeniaInstanceLease final
{
  public:
    XeniaInstanceLease() = default;
    XeniaInstanceLease(const XeniaInstanceLease&) = delete;
    XeniaInstanceLease& operator=(const XeniaInstanceLease&) = delete;
    XeniaInstanceLease(XeniaInstanceLease&&) = delete;
    XeniaInstanceLease& operator=(XeniaInstanceLease&&) = delete;
    ~XeniaInstanceLease();

    // False when another owner holds the lease; this object then holds nothing.
    [[nodiscard]] bool Acquire() noexcept;

  private:
    bool held_ = false;
};

} // namespace x360port

#endif
