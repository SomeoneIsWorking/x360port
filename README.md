# xenon-host

`xenon-host` is a temporary migration source, not the Xbox 360 product
framework. The target framework is `shared/x360port`, which wraps Xenia's
existing x64/A64 Xenon dynarecs and runtime objects. No new title should consume
this library.

## What transfers

The repository has synthetic positive and negative evidence for two reusable
contract families:

- authenticated image identity and layout validation; and
- canonical typed import manifests, including function handlers and variable
  resolvers with kind/library/ordinal/name/guest-address/record-address identity.

These facts move into x360port and are re-proven against its Xenia-backed
runtime boundary. Manifest canonicalization uses big-endian 32-bit integers and
length-prefixed library/name bytes.

## What retires

The following belong only to the abandoned static/generated design and must not
be copied into x360port:

- the precomputed address-only function map and its digest;
- generated entry thunks or a concrete generated PPC ABI;
- a standalone static host dispatcher; and
- the 4 GiB reservation when its only purpose is `window_base + absolute guest
  address` compatibility with generated code.

Xenia owns CPU state, guest address spaces, decoding, lowering, x64/A64 host
emission, executable memory, and translated-block caching. `x360port` owns the
narrow embedding boundary around Xenia `Memory`, `Processor`, `ThreadState`,
and `RawModule`: authenticated images, typed imports, device-memory callbacks,
image-aware runtime overrides, scoped original calls, bounded exits,
invalidation, and explicit handling of Xenia's process-global assumptions.

## Retirement condition

Preserve this repository and its current dirty implementation work until the
transfer is reviewed and equivalent x360port discriminators cover both answers
for each retained contract. Then remove the separate repository if no
independent responsibility remains. Do not keep it as a compatibility layer,
legacy package, or second source of truth.

The portfolio plan is `../jit-common/docs/migration.md`; local transfer details
are in `docs/migration.md`, factual coverage in `docs/project-state.md`, and
ownership in `docs/codemap.md`.
