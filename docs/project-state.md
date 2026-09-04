# Project state

## Comparison baseline

The baseline is the current standalone `xenon-host` prototype for statically
generated Xbox 360 guest code. The target workflow embeds Xenia through
`shared/xenonport`, retains the useful image/import validation contracts there,
and removes this separate repository when it has no independent owner.

| ID | Capability | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | Authenticated image identity/layout contract is synthetically falsified | verified | — | G001 |
| S002 | Typed import manifest and callback-shape contract is synthetically falsified | verified | — | G001 |
| S003 | Image/import contracts are integrated and re-proven in xenonport | missing | S001, S002 | G001, G002 |
| S004 | Generated function-map and static ABI contracts are retired | missing | S003 | G001, G002 |
| S005 | Separate xenon-host repository and consumer references are removed | missing | S003, S004 | G002 |

## Current focus

S003 is the current focus. Preserve the current dirty code and evidence while
moving only the independently reusable authenticated-image and typed-import
contracts to xenonport. Do not add new consumers or extend the static host.

## Capability details

### S001 — Authenticated image contract

Evidence: claim C001 and the contract tests exercise exact image size, base,
entry point, code range, SHA-256, and named refusal paths. The evidence is
synthetic and has not yet been repeated against Xenia `RawModule`.

### S002 — Typed import contract

Evidence: claim C001 and instrument I001 cover canonical function/variable
import fields, distinct handler/resolver callback shapes, one accepted bundle,
and negative mutations. These facts transfer; the generated function-map digest
does not.

### S003 — xenonport integration

Missing capability: `shared/xenonport` does not yet exist, so neither contract
has a runtime Xenia owner or real-module evidence.

Gap: Integrate with Xenia `Memory`, `Processor`, `ThreadState`, and `RawModule`,
then run equivalent positive and negative tests at that production boundary.

### S004 — Generated-contract retirement

Missing capability: The current API and tests still model a precomputed
function map, generated entry code, and a 4 GiB window required by that ABI.

Gap: Do not copy those contracts. Remove them when S003 preserves the reusable
subcontracts and all consumers have moved.

### S005 — Repository removal

Missing capability: This repository and its references still exist.

Gap: After S003 and S004, audit for any independent responsibility. If none
remains, remove the repository and references rather than leaving a deprecated
package or tombstone.
