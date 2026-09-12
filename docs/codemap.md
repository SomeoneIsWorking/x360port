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
| Xenia export/import attachment and callback lifetime | `src/runtime_imports.{hpp,cpp}` | Keep manifest-owned names/tables alive through resolver teardown; bind only validated title-neutral imports. |
| Library implementation tree | `src/` | Add only cohesive title-neutral x360port owners behind public interfaces. |
| Contract falsifiers | `tests/contract_tests.cpp` | Keep synthetic known answers; add production-boundary Xenia cases. |
| Runtime JIT discriminator | `tests/runtime_tests.cpp` | Execute real PPC through the production context and require cache/emission evidence. |
| Pinned Xenia portability regressions | `cmake/XeniaRegressionTests.cmake` | Require and build the fork's focused production-boundary Catch suite and register it in the normal CTest gate. |
| Repository build, dependency preparation, and verification tooling | `tools/{build_support,verify,xenia_dependencies,check_structure}.py` | One locked Python entry point owns host checks, exact Xenia dependency preparation, Clang/Ninja configuration, build, lint, and real synthetic runtime tests. |
| Tooling policy falsifiers | `tools/tests/` | Exercise the shipping dependency-preparation and nested-CMake policy rather than duplicating it in test helpers. |
| First-party compiler diagnostics | `cmake/Warnings.cmake` | Apply the same warning groups with native driver syntax to every first-party library and test target; never alter Xenia's flags. |
| Xenon execution, guest memory, decoding, lowering, host emission, block cache | pinned Xenia revision | Consumed by `RuntimeContext`; CPU/JIT semantics stay in Xenia. |
| Runtime overrides and original calls | `include/x360port/runtime.hpp`, `src/runtime.cpp` | Image-aware entry dispatch through Xenia; scoped original calls suppress only the matching override, with exact-entry invalidation. |
| Title identity, addresses, imports, overrides, policy | consuming title | Never add them here. |

New CPU semantics and JIT machinery go to the Xenia fork/upstream. New shared
embedding behavior goes here only when it is proven by a consumer. UE3-specific
contracts go to `shared/x360ue3`, not this package, once Gears consumes a real
independently authored boundary.
