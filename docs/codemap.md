# Codemap

This map separates the current prototype locations from their target owners.
Capability state is in `docs/project-state.md`; transfer order and deletion are
in `docs/migration.md`.

| Responsibility | Current location | Target owner/location | Entry point / disposition |
|---|---|---|---|
| Authenticated image identity and layout validation | `include/xenon_host/guest_module.hpp`, `src/module_validation.cpp`, `src/digest.cpp` | `shared/xenonport`, around Xenia `RawModule` | Preserve fail-closed identity/layout rules and canonical digests; re-prove on a real Xenia module. |
| Typed import identity and binding validation | `include/xenon_host/{guest_module,title_adapter}.hpp`, `src/module_validation.cpp`, `src/digest.cpp` | `shared/xenonport`, typed import/service boundary | Preserve function-versus-variable callback shape and kind/library/ordinal/name/address/record identity; unknown imports refuse. |
| Xenon CPU execution | absent here | Xenia x64/A64 dynarecs embedded by `shared/xenonport` | Xenia owns decoder, lowering, host emitter, executable memory, and block cache. Never add these here. |
| Runtime override/original-call dispatch | absent here | `shared/xenonport` | Image-aware table; disabled and scoped `super` execute the original guest address through Xenia; mutation invalidates captured call decisions. |
| Device memory and bounded executor exits | absent here | `shared/xenonport` over Xenia runtime objects | Use explicit callbacks/exits; account for Xenia's process-global memory/MMIO/clock assumptions. |
| Precomputed generated function map | `include/xenon_host/guest_module.hpp`, `src/module_validation.cpp`, `src/digest.cpp` | no target owner | Delete; runtime control-flow discovery and code-cache lookup belong to Xenia. |
| Generated ABI window | `include/xenon_host/guest_memory.hpp`, `src/guest_memory*.cpp` | no automatic target owner | Do not migrate solely for generated `window_base + address` compatibility. Preserve only independently required Xbox/Xenia mapping facts in xenonport. |
| Static host composition | `include/xenon_host/host.hpp`, `src/host.cpp` | no target owner | Delete after transferable validators land; xenonport composes Xenia rather than invoking generated entry code. |
| Contract falsifiers | `tests/contract_tests.cpp`, `tests/guest_memory_tests.cpp` | corresponding xenonport tests | Port only tests for retained image/import invariants, with positive and controlled-negative cases. Generated-map/ABI tests retire. |
| Mechanical gates | `tools/check_structure.py`, CTest | xenonport's normal verifier if still applicable | Do not migrate project-name or source-shape policy blindly. |

## Where does new work go?

- XEX image authentication or import validation needed by Xenia integration →
  `shared/xenonport`.
- Xenon instruction semantics, x64/A64 emission, executable memory, or block
  cache → Xenia; contribute to the fork/upstream rather than this repository.
- Gears addresses, import handlers, native implementations, or policies → the
  Gears exact title/revision adapter.
- Generated function inventory, entry thunk, or static ABI compatibility →
  nowhere; remove it with this repository.
