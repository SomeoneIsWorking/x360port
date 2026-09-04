# x360port

`x360port` is the shared Xbox 360 runtime boundary around Xenia. Xenia owns
Xenon decoding, its x64/A64 dynamic recompilers, executable memory, guest
memory, and translated-block caching. This package owns only the narrow
embedding contracts shared by title ports.

The executable slice is a bounded, single-instance Xenia context owning
`Memory`, `Processor`, `ThreadState`, and `RawModule`. It validates an exact
image and import manifest before committing guest memory, maps one raw image at
its authenticated address, binds typed function and variable imports through
Xenia's production export machinery, translates PPC on demand with Xenia's host
dynarec, and calls the cached host code. The runtime test executes a real PPC
leaf plus guest calls and loads through both import kinds, and requires nonzero
emitted host code.

It has no interpreter, generated-code, or fallback executor. x86-64 Linux is
locally verified. CI executes the same synthetic runtime contract on Linux
x86-64, Windows x86-64, and Apple Silicon macOS; a job fails if its runner does
not have the declared architecture. Android arm64-v8a remains unsupported
because there is no APK/runtime packaging owner capable of executing this
contract on an Android runner yet; no compile-only Android job claims runtime
support.

Use the locked canonical verifier against the exact pinned Xenia checkout:

```console
uv run --frozen python tools/verify.py --xenia-source /path/to/xenia
```

The verifier selects Clang and Ninja, refuses build output outside `build/`,
runs Python quality checks, builds the real executor and synthetic fixtures,
and runs CTest including clang-format, clang-tidy, structure checks, and the
Xenia JIT/import runtime test.

The required Xenia revision is
`340aeb13e62bc733b000f23a763d2c3aeda906f8` from the maintained
`SomeoneIsWorking/xenia-canary` `main` branch; configuration refuses any other
revision. `-DX360PORT_VALIDATION_ONLY=ON` builds only the synthetic diagnostic
validator and never claims runtime capability.

Title addresses, identities, imports, overrides, and policies remain in their
title repositories. No generated function map, generated entry ABI, standalone
guest-memory window, or static dispatcher belongs here.
