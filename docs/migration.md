# Migration to x360port

The portfolio authority is `../jit-common/docs/migration.md`. This document owns
only the transfer out of `xenon-host`.

## Transfer sequence

1. Create `shared/x360port` around Xenia `Memory`, `Processor`, `ThreadState`,
   and `RawModule`, using Xenia's existing x64/A64 dynarecs and code cache.
2. Move the authenticated image identity/layout rules into that real module
   loading boundary and prove acceptance plus every retained refusal class.
3. Move canonical typed imports into x360port. Preserve function/variable
   distinction and kind/library/ordinal/name/guest-address/record-address
   identity; unknown or wrongly shaped bindings refuse.
4. Do not copy the precomputed function map, generated entry thunk, static host
   dispatcher, or the 4 GiB window solely required by generated absolute-address
   arithmetic.
5. Migrate consumers and re-run the production-boundary positive and
   controlled-negative tests.
6. Audit this repository for an independent remaining owner. If none exists,
   remove the repository and all references rather than retaining a compatibility
   or legacy layer.

## Evidence boundary

Current contract tests prove synthetic behavior in this implementation. They do
not prove x360port or Xenia integration. Equivalent evidence must exercise the
shipping x360port path. Gears' first consumer proof is real leaf `0x8222E868`,
typed `DbgPrint`, and disabled/enabled/scoped-`super` override dispatch through
Xenia; representative gameplay is a later Gears gate, not this repository's
retirement prerequisite once no consumer remains.

Preserve all current dirty implementation work until it is reviewed against
this split. Do not extend or run the old static product while planning the
transfer.
