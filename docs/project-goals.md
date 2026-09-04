# Project goals

## G001 — Embed Xenia as the shared Xbox 360 executor

### Success conditions

- A bounded context owns Xenia `Memory`, `Processor`, `ThreadState`, and
  `RawModule`, with explicit single-instance or proven isolated-instance rules.
- Non-native guest code executes only through Xenia's x64/A64 dynarecs and
  reports nonzero translated-block work; no gameplay interpreter is linked or
  selectable.
- The A64 dynarec is qualified independently on Apple Silicon macOS and Android
  arm64-v8a, including executable memory, instruction-cache coherence, ABI
  transitions, exception/signal behavior, and packaging.
- Authenticated module loading, typed imports, device-memory callbacks,
  image-aware overrides, scoped original calls, bounded exits, and executable
  invalidation are exercised at the production boundary.

### Constraints

- Xenia retains decoder, lowering, host emitter, executable-memory, guest-memory,
  and block-cache ownership.
- No generated source, function maps, static entry ABI, compatibility host, or
  title-specific policy.

## G002 — Provide reusable fail-closed module and import contracts

### Success conditions

- Exact image digest, size, address range, code range, and entry point are
  validated before title policy or execution.
- Typed function/variable imports seal kind, library, ordinal, name, guest
  address, record address, and exclusive callback shape.
- Positive and controlled-negative tests cover every named refusal and every
  canonical digest field, first synthetically and then through Xenia `RawModule`.
