---
id: C001
kind: claim
status: holds
created: 2026-08-22
tags: xenon,contract,host
depends: src/host.cpp#Host::Run, src/module_validation.cpp#ValidateModule, tests/contract_tests.cpp
---

## Claim

The title-neutral host refuses guest entry unless exact image identity/layout, sorted sealed function and import manifests, exact non-null import bindings, portable title keys, and required capabilities all validate.

## Evidence

xenon_host_contract_tests: 27 checks passed, comprising independent SHA-256 KAT, one acceptance, and every 25 named refusal paths

## What would falsify it

A RunError, Host::Run gate, module/import validation rule, canonical digest, or contract fixture changes without rerunning contract, format, clang-tidy, and structure gates
