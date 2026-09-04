# Project state

## Comparison baseline

The baseline is direct per-title Xenia integration or the retired generated-code
host approach. The target is one reusable Xenia-backed runtime boundary with no
static or interpreter product path.

| ID | Capability | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | Authenticated image/layout validator is synthetically falsified | verified | — | G002 |
| S002 | Typed import manifest/binding validator is synthetically falsified | verified | — | G002 |
| S003 | Xenia runtime objects have one bounded embedding context | missing | — | G001 |
| S004 | Retained validators execute against Xenia `RawModule` | missing | S001, S002, S003 | G001, G002 |
| S005 | Gears leaf/import/override discriminator executes through Xenia | missing | S003, S004 | G001 |

## Current focus

S003 is the current focus. `x360port_validation` is the only implemented
library. There is deliberately no `x360port` executor target, so consumers
cannot mistake synthetic contract preservation for a runnable product.

## Evidence

`x360port_contract_tests` carries independent SHA-256 and canonical-import
known answers, mutations of every canonical import field, accepted function and
variable bindings, every named validation refusal, and wrong callback-shape
controls. This does not prove Xenia integration or guest execution.
