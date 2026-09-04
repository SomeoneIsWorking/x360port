# Codemap

| Responsibility | Current owner | Target entry point |
|---|---|---|
| Module/import schemas and canonical digests | `include/x360port/module_contract.hpp` | Adapt exact parsed Xenia module metadata without copying title policy. |
| Fail-closed image/import validation | `include/x360port/validation.hpp`, `src/{module_validation,digest,validation}.cpp` | Run before title activation or guest execution. |
| Contract falsifiers | `tests/contract_tests.cpp` | Keep synthetic known answers; add production-boundary Xenia cases. |
| Xenon execution, guest memory, decoding, lowering, host emission, block cache | Xenia (not implemented here yet) | Future bounded context over `Memory`, `Processor`, `ThreadState`, `RawModule`. |
| Runtime overrides and original calls | absent | Image-aware dispatch through Xenia; scoped original calls suppress only one override. |
| Title identity, addresses, imports, overrides, policy | consuming title | Never add them here. |

New CPU semantics and JIT machinery go to the Xenia fork/upstream. New shared
embedding behavior goes here only when it is proven by a consumer. UE3-specific
contracts go to `shared/x360ue3`, not this package, once Gears consumes a real
independently authored boundary.
