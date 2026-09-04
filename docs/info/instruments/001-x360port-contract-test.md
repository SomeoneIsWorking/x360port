---
id: I001
kind: instrument
status: trusted
created: 2026-09-04
---

## Instrument

x360port validator contract test

## Validated by

Independent SHA-256 `abc` and canonical import-manifest known answers, six
field mutations, accepted function/variable bindings, controlled negatives for
every named `ValidationError`, and a coverage array that fails when an error has
no test.

## Known failure modes

This tests synthetic descriptors only. It cannot establish Xenia `RawModule`
integration, dynarec execution, override dispatch, or gameplay.
