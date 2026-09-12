#include "runtime_imports.hpp"

#include "guest_execution_budget.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "xenia/base/byte_order.h"
#include "xenia/cpu/export_resolver.h"
#include "xenia/cpu/function.h"
#include "xenia/cpu/raw_module.h"
#include "xenia/cpu/symbol.h"
#include "xenia/memory.h"

namespace x360port
{
namespace
{

[[nodiscard]] bool IsReadable(xe::memory::PageAccess access) noexcept
{
    return access == xe::memory::PageAccess::kReadOnly ||
           access == xe::memory::PageAccess::kReadWrite;
}

[[nodiscard]] bool IsWritable(xe::memory::PageAccess access) noexcept
{
    return access == xe::memory::PageAccess::kReadWrite;
}

[[nodiscard]] bool IsAccessible(xe::Memory* memory, GuestAddress address, std::size_t size,
                                bool write) noexcept
{
    if (memory == nullptr || size > UINT32_MAX ||
        (size != 0U && address > UINT32_MAX - static_cast<GuestAddress>(size - 1U)))
    {
        return false;
    }
    if (size == 0U)
    {
        return true;
    }
    const GuestAddress last = address + static_cast<GuestAddress>(size - 1U);
    auto* heap = memory->LookupHeap(address);
    if (heap == nullptr || memory->LookupHeap(last) != heap)
    {
        return false;
    }
    const auto access = heap->QueryRangeAccess(address, last);
    return write ? IsWritable(access) : IsReadable(access);
}

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, std::string detail)
{
    return RuntimeFailure{error, std::move(detail)};
}

[[nodiscard]] xe::cpu::Export::Type ExportType(ImportKind kind) noexcept
{
    return kind == ImportKind::Function ? xe::cpu::Export::Type::kFunction
                                        : xe::cpu::Export::Type::kVariable;
}

} // namespace

bool GuestImportContext::read_memory(GuestAddress address,
                                     std::span<std::byte> destination) const noexcept
{
    auto* memory = static_cast<xe::Memory*>(memory_);
    if (!IsAccessible(memory, address, destination.size(), false))
    {
        return false;
    }
    if (!destination.empty())
    {
        std::memcpy(destination.data(), memory->TranslateVirtual(address), destination.size());
    }
    return true;
}

bool GuestImportContext::write_memory(GuestAddress address,
                                      std::span<const std::byte> source) const noexcept
{
    auto* memory = static_cast<xe::Memory*>(memory_);
    if (!IsAccessible(memory, address, source.size(), true))
    {
        return false;
    }
    if (!source.empty())
    {
        std::memcpy(memory->TranslateVirtual(address), source.data(), source.size());
    }
    return true;
}

class RuntimeImports::Impl final
{
  public:
    struct OwnedExport final
    {
        OwnedExport(const ImportRequirement& requirement, const ImportBinding& binding,
                    GuestAddress resolved_variable)
            : library(requirement.library), name(requirement.name),
              value(static_cast<std::uint16_t>(requirement.ordinal), ExportType(requirement.kind),
                    name.c_str()),
              kind(requirement.kind), address(requirement.address),
              record_address(requirement.record_address),
              function_handler(binding.function_handler),
              function_context(binding.function_context), resolved_variable(resolved_variable)
        {
        }

        std::string library;
        std::string name;
        xe::cpu::Export value;
        ImportKind kind;
        GuestAddress address;
        GuestAddress record_address;
        ImportFunctionHandler function_handler;
        void* function_context;
        GuestAddress resolved_variable;
        xe::Memory* memory = nullptr;
    };

    struct OwnedTable final
    {
        std::string library;
        std::vector<std::unique_ptr<OwnedExport>> owned_exports;
        std::vector<xe::cpu::Export*> exports_by_ordinal;
    };

    static void DispatchFunction(xe::cpu::ppc::PPCContext* call_context, xe::kernel::KernelState*,
                                 void* callback_context) noexcept
    {
        auto& owned_export = *static_cast<OwnedExport*>(callback_context);
        std::array<std::uint64_t, GuestImportContext::argument_count> arguments{};
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            arguments[index] = call_context->r[3 + index];
        }
        GuestImportContext context(arguments, owned_export.memory);
        owned_export.function_handler(context, owned_export.function_context);
        if (const auto refusal = context.refusal())
        {
            MarkGuestImportRefused(owned_export.library, owned_export.value.ordinal, *refusal);
            return;
        }
        call_context->r[3] = context.return_value();
    }

    std::vector<std::unique_ptr<OwnedTable>> tables;
};

RuntimeImports::RuntimeImports(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

RuntimeImports::~RuntimeImports() = default;

RuntimeImportsCreateResult RuntimeImports::Create(std::span<const ImportRequirement> requirements,
                                                  std::span<const ImportBinding> bindings)
{
    auto impl = std::make_unique<Impl>();
    for (std::size_t index = 0; index < requirements.size(); ++index)
    {
        const ImportRequirement& requirement = requirements[index];
        const ImportBinding& binding = bindings[index];
        GuestAddress resolved_variable = 0;
        if (requirement.kind == ImportKind::Variable)
        {
            resolved_variable = binding.variable_resolver(binding.variable_resolution_context);
            if (resolved_variable == 0)
            {
                return {nullptr,
                        Failure(RuntimeError::VariableResolutionFailed,
                                "variable import resolver returned the null guest address")};
            }
        }

        if (impl->tables.empty() || impl->tables.back()->library != requirement.library)
        {
            auto table = std::make_unique<Impl::OwnedTable>();
            table->library = requirement.library;
            impl->tables.push_back(std::move(table));
        }
        Impl::OwnedTable& table = *impl->tables.back();
        const std::size_t ordinal = requirement.ordinal;
        if (table.exports_by_ordinal.size() <= ordinal)
        {
            table.exports_by_ordinal.resize(ordinal + 1U, nullptr);
        }
        auto owned_export =
            std::make_unique<Impl::OwnedExport>(requirement, binding, resolved_variable);
        table.exports_by_ordinal[ordinal] = &owned_export->value;
        table.owned_exports.push_back(std::move(owned_export));
    }
    return {std::unique_ptr<RuntimeImports>(new RuntimeImports(std::move(impl))), {}};
}

RuntimeFailure RuntimeImports::Attach(xe::cpu::ExportResolver& resolver, xe::cpu::RawModule& module,
                                      xe::Memory& memory)
{
    for (const auto& table : impl_->tables)
    {
        resolver.RegisterTable(table->library, &table->exports_by_ordinal);
        for (const auto& owned_export : table->owned_exports)
        {
            owned_export->memory = &memory;
            const std::uint16_t ordinal = owned_export->value.ordinal;
            if (owned_export->kind == ImportKind::Function)
            {
                resolver.SetFunctionMapping(table->library, ordinal, Impl::DispatchFunction,
                                            owned_export.get());
                xe::cpu::Function* function = nullptr;
                const xe::cpu::Symbol::Status status =
                    module.DeclareFunction(owned_export->address, &function);
                if (status != xe::cpu::Symbol::Status::kNew || function == nullptr ||
                    !function->is_guest())
                {
                    return Failure(RuntimeError::ImportAttachmentFailed,
                                   "Xenia refused a validated function import address");
                }
                auto* thunk = memory.TranslateVirtual(owned_export->address);
                xe::store_and_swap<std::uint32_t>(thunk, 0x44000042U);
                xe::store_and_swap<std::uint32_t>(thunk + 4U, 0x4E800020U);
                xe::store_and_swap<std::uint32_t>(thunk + 8U, 0x60000000U);
                xe::store_and_swap<std::uint32_t>(thunk + 12U, 0x60000000U);
                function->set_end_address(owned_export->address + 12U);
                function->set_name(owned_export->name);
                static_cast<xe::cpu::GuestFunction*>(function)->SetupExtern(
                    owned_export->value.function_data.trampoline, &owned_export->value,
                    owned_export->value.function_data.callback_context);
                function->set_status(xe::cpu::Symbol::Status::kDeclared);
            }
            else
            {
                resolver.SetVariableMapping(table->library, ordinal,
                                            owned_export->resolved_variable);
                auto* record = memory.TranslateVirtual(owned_export->record_address);
                xe::store_and_swap<std::uint32_t>(record, owned_export->value.variable_ptr);
            }
        }
    }
    return {};
}

} // namespace x360port
