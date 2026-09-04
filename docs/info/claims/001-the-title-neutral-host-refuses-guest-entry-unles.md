---
id: C001
kind: claim
status: holds
created: 2026-08-22
tags: xenon,contract,host
depends: src/host.cpp#Host::Run, src/module_validation.cpp#ValidateModule, tests/contract_tests.cpp
---

## Claim

The current prototype's reusable subcontracts fail closed on exact image identity/layout and on typed import identity and callback shape, including function versus variable imports and kind/library/ordinal/name/guest-address/record-address fields. Its generated function-map check is implementation evidence only and is not an x360port requirement.

## Evidence

xenon_host_contract_tests: 44 checks passed, comprising independent SHA-256 and canonical-import known answers, six import-digest field discriminators, one acceptance covering both binding kinds, every 28 named RunError refusal category, and additional address/callback-shape discriminators. This is synthetic xenon-host evidence; x360port must re-prove the retained image/import subset through Xenia.

## What would falsify it

An image/import validation rule, canonical digest, callback shape, or cited fixture changes without re-running the relevant tests, or x360port cannot reproduce a retained positive/negative case. The precomputed function map may be deleted without falsifying the narrowed claim.
