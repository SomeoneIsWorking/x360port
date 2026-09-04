---
id: I002
kind: instrument
status: trusted
created: 2026-09-04
---

## Instrument

x360port Xenia runtime contract test

## Validated by

The production `RuntimeContext` maps an authenticated big-endian PPC leaf,
requires Xenia to produce non-null machine code with a nonzero byte count,
observes guest return value 42, and demonstrates the other cache answer: the
first call increments translation count while the second does not. Controlled
negatives refuse a concurrent fixed mapping and an entry outside authenticated
code. Context teardown is followed by a reload and execution at the same guest
address. A second authenticated image executes a real guest call through a
typed Xenia function export and a real guest load through a Xenia variable
export. Controlled negatives prove null variable resolution leaves the context
reusable, and binding/manifest lifetime ends before execution.

## Known failure modes

This proves an asset-free x64 `RawModule` and import boundary only. It does not
establish title loading, overrides, invalidation, non-returning execution
bounds, A64 execution, device callbacks, or gameplay.
