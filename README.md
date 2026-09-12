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
dynarec, and calls the cached host code. Every translated public guest
execution has a finite basic-block budget and propagates exhaustion across
nested guest calls as a typed failure. The runtime test executes a real PPC
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
Xenia JIT/import runtime test. Its Ninja build reports all reachable independent
build failures in one run; any build failure stops verification before CTest.

The runtime build also provides `x360-xex-inspect`, which accepts a user-owned
XEX2, refuses malformed container and payload bounds, and emits a strict JSON
inspection plus the canonical normalized image when requested with
`--image-out`. It is an input/identity boundary only; it does not launch a title
or provide a gameplay fallback.

The required Xenia revision is
`f024c152d8200bfe5a0f3db11e4fdcd4cd95cca5` from the maintained
`SomeoneIsWorking/xenia-canary` `main` branch; configuration refuses any other
revision. `-DX360PORT_VALIDATION_ONLY=ON` builds only the synthetic diagnostic
validator and never claims runtime capability.

Title addresses, identities, imports, overrides, and policies remain in their
title repositories. No generated function map, generated entry ABI, standalone
guest-memory window, or static dispatcher belongs here.

Title adapters may request bounded caller-owned guest allocations through
`RuntimeContext::AllocateGuestMemory`, initialize them with
`WriteGuestMemory`, and release them with `ReleaseGuestMemory`. The shared
owner tracks each complete allocation and rejects writes or releases that do
not stay within or exactly match a live range; it does not expose an
unbounded guest-memory window or title policy.
