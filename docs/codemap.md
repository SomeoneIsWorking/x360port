# Codemap

This is the title-neutral validation and composition seam between locally generated Xbox 360 guest
code and a title port. Generated PPC ABI/layout code remains on the title side; platform services
remain absent until implemented as real shared subsystems. The core gap is execution against a real
title module: this repository currently proves only the host contract with synthetic data.

Status vocabulary: **verified-synthetic** means built and falsified by synthetic contract tests;
**absent** means deliberately not implemented and never represented as success.

## Subsystems

| Subsystem | Status | Where | Gap/next |
|---|---|---|---|
| Guest module API | **verified-synthetic** | `include/xenon_host/guest_module.hpp`, `GuestModule` | Connect a locally generated exact title bridge without adding generated files here. |
| Title adapter API | **verified-synthetic** | `include/xenon_host/title_adapter.hpp`, `TitleAdapter` | First consumer must supply real capability and import implementations. |
| Contract validation | **verified-synthetic** | `src/module_validation.cpp`, `ValidateModule` | Re-verify against the first real generated manifest. |
| Host composition | **verified-synthetic** | `src/host.cpp`, `Host::Run` | No lifecycle beyond validated adapter entry exists yet. |
| SHA-256/canonical manifests | **verified-synthetic** | `src/digest.cpp`, `HashBytes` | Canonical forms cover guest addresses and library/ordinal/name imports. |
| Mechanical gates | **verified-synthetic** | `tools/check_structure.py`, ctest `structure` | Enforces 500-line ownership and rejects generated/title dependencies in shared product code. |
| Kernel services | **absent** | capability `KernelServices` | Add only evidenced services with trapping unknown imports. |
| Graphics | **absent** | capability `Graphics` | No renderer or null renderer exists. |
| Audio | **absent** | capability `Audio` | No audio backend exists. |
| Input | **absent** | — | If added, one host device owner publishes neutral snapshots. |

## Source tree

```text
include/  —  249 lines, 3 files
└─ xenon_host/  249 lines, 3 files  # public GuestModule, TitleAdapter, and Host interfaces
src/      —  487 lines, 4 files     # digest, validation, and composition implementations
tests/    —  309 lines, 1 file      # synthetic acceptance and every named refusal path
tools/    —  105 lines, 1 file      # mechanical source-structure/dependency gate
docs/                # project coverage map
```

## Where is X?

- Exact module acceptance/refusal: `src/module_validation.cpp`, `ValidateModule`
- Exact import binding coverage: `src/module_validation.cpp`, `ValidateImports`
- Final pre-entry capability gate: `src/host.cpp`, `Host::Run`
- Opaque generated-code seam: `include/xenon_host/guest_module.hpp`, `GuestEntryThunk`
- Contract falsifier: `tests/contract_tests.cpp`
