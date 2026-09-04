---
id: C001
kind: claim
status: holds
created: 2026-09-04
tags: x360port,module,imports,validation
depends: src/module_validation.cpp#ValidateModule, src/module_validation.cpp#ValidateImports, tests/contract_tests.cpp
---

## Claim

The validator fails closed on exact image identity/layout, including Xenia's
64 KiB guest-image base alignment, and typed import identity and callback shape,
including function versus variable imports and
kind/library/ordinal/name/guest-address/record-address fields.

## Evidence

`x360port_contract_tests` exercises independent digest known answers, mutations
of every canonical import field, accepted module and binding descriptors, every
named `ValidationError`, and wrong address/callback-shape controls.

## What would falsify it

A validation rule, digest, callback shape, or fixture changes without the
corresponding positive and negative tests, or the Xenia-backed boundary cannot
reproduce a retained case.
