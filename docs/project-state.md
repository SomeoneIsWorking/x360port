# Project state

## Comparison baseline

The baseline is direct per-title Xenia integration or the retired generated-code
host approach. The target is one reusable Xenia-backed runtime boundary with no
static or interpreter product path.

| ID | Capability | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | Authenticated image/layout validator is synthetically falsified | verified | — | G002 |
| S002 | Typed import manifest/binding validator is synthetically falsified | verified | — | G002 |
| S003 | Xenia runtime objects have one bounded embedding context | missing | — | G001 |
| S004 | Retained validators execute against Xenia `RawModule` | missing | S001, S002, S003 | G001, G002 |
| S005 | Gears leaf/import/override discriminator executes through Xenia | missing | S003, S004 | G001 |
| S006 | Xenia A64 execution is qualified on Apple Silicon macOS | missing | S003, S004 | G001 |
| S007 | Xenia A64 execution is qualified on Android arm64-v8a | missing | S003, S004 | G001 |

## Current focus

S003 is the current focus. `x360port_validation` is the only implemented
library. There is deliberately no `x360port` executor target, so consumers
cannot mistake synthetic contract preservation for a runnable product.

## Capability details

### S001 — image validation

Evidence: `x360port_contract_tests` carries independent SHA-256 known answers
and mutations of every authenticated image/layout field.

### S002 — import validation

Evidence: the same production validator test covers accepted function and
variable imports, every sealed manifest field, every named refusal, and wrong
callback-shape controls.

### S003 — Xenia context

Missing capability: no owner yet composes Xenia `Memory`, `Processor`,
`ThreadState`, and `RawModule` with explicit instance lifetime.

### S004 — Xenia-backed validation

Missing capability: apply the verified synthetic validators to a real Xenia
`RawModule` in the future execution context.

### S005 — Gears discriminator

Missing capability: execute the authenticated Gears leaf/import/override
round-trip through Xenia with nonzero dynarec work and no interpreter link.

### S006 — Apple Silicon macOS

Missing capability: qualify Xenia's A64 dynarec on Apple Silicon macOS,
including executable-memory protection, instruction-cache coherence, host ABI,
exception behavior, and representative gameplay.

### S007 — Android arm64-v8a

Missing capability: qualify Xenia's A64 dynarec in the Android arm64-v8a
product package, including executable-memory protection, instruction-cache
coherence, host ABI, signal behavior, sustained execution, and representative
gameplay.
