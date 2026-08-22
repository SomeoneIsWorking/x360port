# xenon-host

`xenon-host` is a title-neutral contract boundary for statically recompiled Xbox 360 games. It is
not an emulator, a kernel, or a renderer. A title bridge supplies one exact guest image, its opaque
entry thunks, its import requirements, and the host capabilities it genuinely implements.

The host validates the whole bundle before guest entry:

- exact image byte count, 32-bit layout, entry point, and SHA-256;
- a 32-byte-aligned 4 GiB virtual guest window, with only the sealed image range initially committed
  and loaded at its full 32-bit guest address;
- exact aligned code range and strictly sorted address-to-thunk map, including its count and digest;
- exact sorted library/ordinal import manifest, including its count and digest;
- exact one-for-one non-null host bindings for that manifest; and
- explicit adapter capabilities required by the caller.

Any mismatch is a named refusal. There are no successful kernel placeholders, renderer stubs, or
title-specific generated symbols in this repository.

## Build and verify

```sh
CXX=clang++ cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Clang, `clang-format`, and `clang-tidy` are required. The library has no runtime dependency on
Alchemy, Gears, SDL, XenonRecomp, or proprietary/generated game code.

## Title integration

Implement `xenon_host::GuestModule` in a generated or ignored title bridge and implement
`xenon_host::TitleAdapter` in the title port. Keep the concrete PPC ABI on the title side: the
shared interface sees only `GuestEntryThunk(void*)`. A future XenonRecomp split should expose
portable ABI types from `ppc_abi.h` while retaining layout and lookup data in generated
`ppc_runtime.h`; neither header belongs here.

The title adapter must bind every import and advertise only real capabilities. `RunRequest` always
requires guest memory by default; add every other capability needed for a run. `Host::Run` refuses
before guest entry if any are absent. `ValidatedGuestModule::Memory()` exposes the live
`GuestMemory`; generated entry thunks receive `GuestMemory::WindowBase()`, while native services use
the overflow-checked `Translate(GuestMemoryRange)` seam. Physical-memory aliases and guest heaps are
not implemented yet and must not be inferred from the image mapping.
