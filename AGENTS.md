# x360port working contract

Read `docs/project-goals.md`, `docs/project-state.md`, and `docs/codemap.md`
before changing this package.

Xenia is the CPU runtime. Reuse its x64/A64 Xenon dynarecs, `Memory`,
`Processor`, `ThreadState`, `RawModule`, executable-memory ownership, and block
cache. Do not implement or expose a gameplay interpreter, offline translator,
generated guest source, precomputed function map, static dispatcher, or a
second code cache.

The currently built `x360port_validation` library is an evidence-preserving
slice, not an executor. Do not add an `x360port` product target until it owns a
real Xenia-backed execution context and can report nonzero JIT work. Unknown or
ambiguous image/import/override identity fails closed.

Keep title policy out. Exact Gears addresses and bindings belong to Gears.
Reusable UE3 contracts belong to `shared/x360ue3` only after a real contract is
consumed by Gears; never copy or depend on the local `shared/ue3` sources.
