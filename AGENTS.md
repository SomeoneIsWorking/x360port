# x360port working contract

Read `docs/project-goals.md`, `docs/project-state.md`, and `docs/codemap.md`
before changing this package.

Xenia is the CPU runtime. Reuse its x64/A64 Xenon dynarecs, `Memory`,
`Processor`, `ThreadState`, `RawModule`, executable-memory ownership, and block
cache. Do not implement or expose a gameplay interpreter, offline translator,
generated guest source, precomputed function map, static dispatcher, or a
second code cache.

The `x360port` target is the real Xenia-backed executor. Its current vertical
slice accepts one authenticated raw image, attaches typed function and variable
imports through Xenia's export machinery, translates PPC on first execution,
and reuses Xenia's code cache. Keep `x360port_validation` only as the diagnostic
contract target; it is never a substitute backend. Unknown or ambiguous
image/import/override identity fails closed.

Xenia revision `657d521af176e07a1290524310855c6eb966970c` from the maintained
`SomeoneIsWorking/xenia-canary` `main` branch is the executable
contract. Do not make that pin caller-overridable. The public Pimpl boundary
must not expose Xenia types or libstdc++ ABI choices. On GNU libstdc++ Debug
builds, any translation unit constructing Xenia objects must match Xenia's
`_GLIBCXX_DEBUG` container ABI.

Keep title policy out. Exact Gears addresses and bindings belong to Gears.
Reusable UE3 contracts belong to `shared/x360ue3` only after a real contract is
consumed by Gears; never copy or depend on the local `shared/ue3` sources.

`tools/verify.py` is the canonical build and verification owner. CI and local
maintainers must enter it through the locked `uv` environment and must run its
real synthetic JIT test on the host architecture; a compile-only or
validation-only job is not a runtime-success substitute.
