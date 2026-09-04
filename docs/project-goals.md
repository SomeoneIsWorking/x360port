# Project goals

## G001 — Transfer reusable Xbox 360 validation contracts to xenonport

### Why

Authenticated runtime images and typed imports remain necessary when Gears and
other titles execute through Xenia. They belong beside the runtime that consumes
them, not in a static-host library.

### Success conditions

- `shared/xenonport` validates exact image identity/layout through Xenia
  `RawModule` before title policy or guest entry.
- It validates canonical typed function and variable imports, including
  kind/library/ordinal/name/guest-address/record-address identity and callback
  shape, with positive and controlled-negative tests.
- The transferred contracts have one owner and no consumer depends on
  `xenon-host`.

### Constraints

- Xenia retains ownership of Xenon execution, guest memory, host-code emission,
  executable memory, and its block cache.
- Do not transfer the generated function map, generated entry ABI, static
  dispatcher, or ABI-only 4 GiB window.

### Non-goals

- Turning `xenon-host` into a dynarec, preserving it as a compatibility layer,
  or defining title-specific policy here.

## G002 — Remove xenon-host after its last responsibility moves

### Why

A second shared boundary with overlapping image/import contracts would recreate
the ownership split the migration is intended to eliminate.

### Success conditions

- Equivalent xenonport tests re-prove every transferred invariant and its
  negative discriminator.
- Generated-only contracts and tests are explicitly retired rather than copied.
- If no independent owner remains, the separate repository and all consumer
  references are removed; no legacy/tombstone package survives.

### Constraints

- Preserve current code and dirty work until the transfer is reviewed.
- Repository deletion follows consumer migration and equivalent evidence; it is
  not performed merely because the new architecture is documented.

### Non-goals

- Shipping or maintaining two public APIs for the same validation boundary.
