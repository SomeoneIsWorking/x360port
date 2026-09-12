# Codemap

| Responsibility | Current owner | Target entry point |
|---|---|---|
| Module/import schemas and canonical digests | `include/x360port/module_contract.hpp` | Adapt exact parsed Xenia module metadata without copying title policy. |
| PE image layout adapter | `include/x360port/pe_image.hpp`, `src/pe_image.cpp` | Validate an Xbox 360 PE container's geometry and map its sections into the flat guest image contract; preserve source and runtime-image digests separately. |
| Checked XEX2 inspection | `include/x360port/xex_inspect.hpp`, `src/xex_inspect.cpp`, `tools/xex_inspect.cpp` | Own fail-closed XEX2 header/payload handoff to the pinned Xenia loader, canonical import-record/function-stub normalization, PE mapping, execution metadata, logical imports, and helper-pattern evidence. It has no title identity or import policy. |
| Fail-closed image/import validation | `include/x360port/validation.hpp`, `src/{module_validation,digest,validation}.cpp` | Run before title activation or guest execution. |
| Xenia context and guest call ownership | `include/x360port/runtime.hpp`, `src/runtime.cpp` | Extend through narrow typed runtime contracts, including finite translated-block limits propagated across nested guest calls; keep Xenia types behind the Pimpl boundary. |
| Title-owned guest allocations | `include/x360port/runtime.hpp`, `src/guest_memory.{hpp,cpp}` | Allocate, initialize, write, and release bounded guest ranges through Xenia's system heap; retain ownership metadata so title setup cannot write outside a live allocation. |
| Device-backed guest memory | `include/x360port/runtime.hpp`, `src/device_dispatch.{hpp,cpp}` | Register title-supplied masked ranges through Xenia `Memory`; own callback lifetime and access telemetry without importing title policy. |
| Executable translation invalidation | `include/x360port/runtime.hpp`, `src/executable_invalidation.{hpp,cpp}` | Validate title-reported PPC write ranges, automatically watch the authenticated virtual code range through Xenia, exit an active translated call after a watched guest store, drain coalesced writes before the next guest-call boundary, remove touched Xenia functions, and retain observation/invalidation telemetry. |
| Xenia export/import attachment and callback lifetime | `src/runtime_imports.{hpp,cpp}`, `src/guest_execution_budget.hpp`, `include/x360port/validation.hpp` | Keep manifest-owned names/tables alive through resolver teardown; bind only validated title-neutral imports and expose register arguments, return propagation, bounded guest-memory access, and typed fail-closed refusal through `GuestImportContext`. |
| XAM controller guest services | `include/x360port/xam_input.hpp`, `src/xam_input.cpp` | Bind `xam.xex` ordinals 400/401, serialize the 20-byte capabilities and 16-byte state records into checked guest memory, and return connection/argument status; the consuming title supplies device capabilities, a coherent state snapshot, and user policy. |
| Library implementation tree | `src/` | Add only cohesive title-neutral x360port owners behind public interfaces. |
| Contract falsifiers | `tests/contract_tests.cpp` | Keep synthetic known answers; add production-boundary Xenia cases. |
| Runtime JIT discriminator | `tests/runtime_tests.cpp` | Execute real PPC through the production context and require cache/emission evidence. |
| Import/JIT integration discriminator | `tests/import_runtime_tests.cpp` | Exercise bound function and variable exports, bounded guest-memory access, direct and nested host-service refusal, accounting, and recovery through the production context. |
| Pinned Xenia portability regressions | `cmake/XeniaRegressionTests.cmake` | Require and build the fork's focused production-boundary Catch suite and register it in the normal CTest gate. |
| Repository build, dependency preparation, and verification tooling | `tools/{build_support,verify,xenia_dependencies,check_structure}.py` | One locked Python entry point owns host checks, exact Xenia dependency preparation, Clang/Ninja configuration, build, lint, and real synthetic runtime tests. |
| Tooling policy falsifiers | `tools/tests/` | Exercise the shipping dependency-preparation and nested-CMake policy rather than duplicating it in test helpers. |
| First-party compiler diagnostics | `cmake/Warnings.cmake`, `tests/warnings/probe.cpp` | Apply the same warning groups with native driver syntax to every first-party library and test target; test the warning policy without altering Xenia's flags. |
| Xenon execution, guest memory, decoding, lowering, host emission, block cache, bounded fallback | pinned Xenia revision | Consumed by `RuntimeContext`; CPU/JIT and bounded fallback semantics stay in Xenia. |
| Runtime overrides and original calls | `include/x360port/runtime.hpp`, `src/runtime.cpp`, `src/override_dispatch.{hpp,cpp}`, `src/guest_execution_budget.hpp` | Bind authenticated entries to Xenia guest-call redirects; host entry and cached guest callers use one override table, while scoped original calls enter the translated body directly. Guest callback failures exit the active bounded call with their typed reason. |
| Title identity, addresses, imports, overrides, policy | consuming title | Never add them here. |

New CPU semantics and JIT machinery go to the Xenia fork/upstream. New shared
embedding behavior goes here only when it is proven by a consumer. UE3-specific
contracts go to `shared/x360ue3`, not this package, once Gears consumes a real
independently authored boundary.
