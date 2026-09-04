---
id: C002
kind: claim
status: holds
created: 2026-08-22
tags:
depends: include/xenon_host/guest_memory.hpp#GuestMemory, src/guest_memory.cpp#GuestMemoryLoader::Load, src/guest_memory_posix.cpp#GuestMemoryLoader::PlatformOperations, src/host.cpp#HostRunner::Run, tests/guest_memory_tests.cpp, tests/contract_tests.cpp
---

## Claim

The current static-host prototype supplies its generated ABI with a 32-byte-aligned 4 GiB virtual window, loads the sealed image at its full guest address, bounds native translation without overflow, and refuses reserve/alignment/commit failures before generated entry. This is historical generated-ABI evidence, not a requirement for x360port.

## Evidence

Clang normal and ASan/UBSan CTest suites both passed 6/6; guest_memory exercised real low and top-of-32-bit-space mappings plus 19 load/translation checks, and contract exercised all 28 named Host::Run errors including injected reservation/alignment/commit failures.

## What would falsify it

The cited implementation or tests no longer reproduce the recorded generated-ABI behavior. Deleting this contract during retirement does not falsify the historical result; x360port uses Xenia's memory ownership and preserves only independently required mapping facts.
