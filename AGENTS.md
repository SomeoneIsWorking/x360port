# x360port working contract

Module map: `docs/codemap.md`. Execution policy: the `dynarec-port`, `dynarec-runtime` and
`dynarec-overrides` skills.

Xenia is the CPU runtime. Reuse its x64/A64 Xenon dynarecs, `Memory`, `Processor`, `ThreadState`,
`RawModule`, executable-memory ownership, and block cache; never add a second code cache.

The `x360port` target is the real Xenia-backed executor. Its current vertical slice accepts one
authenticated raw image, attaches typed function and variable imports through Xenia's export
machinery, translates PPC on first execution, and reuses Xenia's code cache. `x360port_validation`
is only the diagnostic contract target; it is never a substitute backend. Unknown or ambiguous
image/import/override identity fails closed.

The Xenia revision pinned in `dependencies.json`, from the maintained `SomeoneIsWorking/xenia-canary`
`main` branch, is the executable contract. Do not make that pin caller-overridable. The public Pimpl
boundary must not expose Xenia types or libstdc++ ABI choices. On GNU libstdc++ Debug builds, any
translation unit constructing Xenia objects must match Xenia's `_GLIBCXX_DEBUG` container ABI.

Keep title policy out. Exact Gears addresses and bindings belong to Gears. Reusable UE3 contracts
belong to `shared/x360ue3` only after a real contract is consumed by Gears; never copy or depend on
the local `shared/ue3` sources.

Build and full verifier: `uv run --frozen python tools/verify.py`. It runs the real synthetic JIT
test on the host architecture.
