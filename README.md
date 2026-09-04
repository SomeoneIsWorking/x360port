# x360port

`x360port` is the shared Xbox 360 runtime boundary around Xenia. Xenia owns
Xenon decoding, its x64/A64 dynamic recompilers, executable memory, guest
memory, and translated-block caching. This package owns only the narrow
embedding contracts shared by title ports.

The first executable slice is a bounded, single-instance Xenia context owning
`Memory`, `Processor`, `ThreadState`, and `RawModule`. It validates an exact
image and import manifest before committing guest memory, maps one import-free
raw image at its authenticated address, translates PPC on demand with Xenia's
host dynarec, and calls the cached host code. The runtime test executes a real
two-instruction PPC leaf and requires nonzero emitted host code.

The runtime deliberately refuses non-empty import manifests until typed
function and variable callbacks are attached to Xenia's export machinery. It
has no interpreter, generated-code, or fallback executor. x86-64 is verified;
Xenia's A64 backend is selected on arm64 hosts but remains unqualified on Apple
Silicon and Android.

Configure against the exact pinned Xenia checkout and build with Ninja:

```console
cmake -S . -B build/runtime -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DX360PORT_XENIA_SOURCE_DIR=/path/to/xenia
cmake --build build/runtime --target x360port_runtime_tests
ctest --test-dir build/runtime --output-on-failure
```

The required Xenia revision is
`1150303fe1694edfc2de8c6443750952e9d5b8bc`; configuration refuses any other
revision. `-DX360PORT_VALIDATION_ONLY=ON` builds only the synthetic diagnostic
validator and never claims runtime capability.

Title addresses, identities, imports, overrides, and policies remain in their
title repositories. No generated function map, generated entry ABI, standalone
guest-memory window, or static dispatcher belongs here.
