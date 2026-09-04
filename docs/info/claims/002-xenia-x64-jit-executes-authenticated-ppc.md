---
id: C002
kind: claim
status: holds
created: 2026-09-04
tags: x360port,xenia,dynarec,x64
depends: include/x360port/runtime.hpp, src/runtime.cpp, tests/runtime_tests.cpp
---

## Claim

On x86-64, x360port loads an authenticated import-free guest image into a real
Xenia `RawModule`, translates PPC with Xenia's x64 backend on first execution,
and reuses the translated host function on the second execution.

## Evidence

`x360port_runtime_tests` executes `li r3, 42; blr`, observes 42 twice, requires
one translated function and a nonzero machine-code byte count, and successfully
recreates the context before reloading the same fixed guest range.

## What would falsify it

The pinned Xenia revision, backend selection, authenticated loading, call ABI,
cache accounting, guest-memory lifetime, or PPC fixture changes without this
production-boundary test returning both the cold and cached answers.
