# Working agreement

Read `docs/codemap.md` before changing a subsystem and update it in the same change. This repository
is a shared, title-neutral boundary: it must not depend on Alchemy, Gears, a ROM/XEX, generated
recompiler output, `ppc_config.h`, or per-title `sub_*` symbols.

Ownership follows Dusklight's composition pattern without copying its platform implementation:

- `include/xenon_host/guest_module.hpp` owns the generated-module contract.
- `include/xenon_host/guest_memory.hpp` owns the generated ABI's 4 GiB guest-address window and
  bounded host translation; `src/guest_memory.cpp` owns image loading and
  `src/guest_memory_posix.cpp` owns the POSIX reservation implementation.
- `include/xenon_host/title_adapter.hpp` owns the title/host capability and import-binding contract.
- `src/module_validation.cpp` owns fail-closed validation.
- `src/host.cpp` composes validation and invokes the adapter only after acceptance.

Do not add fake-success kernel services, null renderers presented as functional, or permissive
fallbacks. An absent subsystem is an absent capability and a named `Host::Run` refusal. If input is
later added, xenon-host owns the host device/SDL boundary and publishes a neutral snapshot; engine
code consumes that snapshot and must not create a second SDL owner.

The generated Xenon ABI indexes `window_base + full_32_bit_guest_address`; never replace the 4 GiB
reservation with an exact-size image allocation or derive a pointer before the image base. Physical
RAM aliases and guest heaps are separate future owners, not implicit behavior of image loading.

Develop and test with Clang (AppleClang included); other compilers configure with a warning, not a
ban. Run the full CTest suite, including format, clang-tidy, contract refusal
coverage, and the 500-line structure gate. Project automation is Python; this library has no
`run.sh` because it is not a runnable product.
