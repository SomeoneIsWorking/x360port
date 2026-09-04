---
id: C003
kind: claim
status: holds
created: 2026-09-04
tags: x360port,xenia,imports,exports
depends: src/runtime_imports.cpp, src/runtime.cpp, tests/runtime_tests.cpp
---

## Claim

Authenticated typed function and variable imports cross Xenia's production
export resolver and guest syscall/load paths without a generated dispatcher or
function-pointer cast, and their backing names, tables, mappings, and callbacks
remain valid for the runtime context lifetime.

## Evidence

`x360port_runtime_tests` executes guest PPC through both import kinds, checks
the callback count and independent return/load values, refuses a null variable
resolution before guest allocation, retries that pre-mutation refusal on the
same context, and executes only after the caller-owned module/bindings are gone.
The maintained Xenia fork separately executes a real kExtern guest-to-host thunk
with distinct callback userdata and verifies guest continuation.

## What would falsify it

The Xenia export callback ABI, resolver/table ownership, XEX thunk encoding,
guest variable record mapping, manifest validation, or destruction order
changes without both production-boundary import paths retaining their checked
results and controlled negative behavior.
