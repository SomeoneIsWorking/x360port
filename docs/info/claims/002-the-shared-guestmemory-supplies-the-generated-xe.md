---
id: C002
kind: claim
status: holds
created: 2026-08-22
tags:
depends: include/xenon_host/guest_memory.hpp#GuestMemory, src/guest_memory.cpp#GuestMemoryLoader::Load, src/guest_memory_posix.cpp#GuestMemoryLoader::PlatformOperations, src/host.cpp#HostRunner::Run, tests/guest_memory_tests.cpp, tests/contract_tests.cpp
---

## Claim

The shared GuestMemory supplies the generated Xenon ABI with a 32-byte-aligned 4 GiB virtual window, commits and loads the exact sealed image at its full guest address, bounds native translation without overflow, and refuses reserve/alignment/commit failures before guest entry.

## Evidence

Clang normal and ASan/UBSan CTest suites both passed 6/6; guest_memory exercised real low and top-of-32-bit-space mappings plus 19 load/translation checks, and contract exercised all 28 named Host::Run errors including injected reservation/alignment/commit failures.

## What would falsify it

GuestMemory window size/alignment/translation, POSIX reserve/commit/release, image loading, ValidatedGuestModule memory handoff, generated PPC addressing ABI, or either guest-memory/contract test changes without rerunning normal and sanitizer CTest.
