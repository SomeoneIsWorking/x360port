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
| Guest memory | **verified-synthetic** | `include/xenon_host/guest_memory.hpp`, `GuestMemory`; `src/guest_memory.cpp`, `GuestMemoryLoader::Load` | Real Linux and sanitizer tests reserve a 32-byte-aligned 4 GiB window, commit/load the exact image pages, and falsify range/reserve/alignment/commit failures. Physical aliases and heaps remain absent. |
| Host composition | **verified-synthetic** | `src/host.cpp`, `Host::Run` | Owns guest memory through adapter entry; no lifecycle beyond that validated entry exists yet. |
| SHA-256/canonical manifests | **verified-synthetic** | `src/digest.cpp`, `HashBytes` | Canonical forms cover guest addresses and library/ordinal/name imports. |
| Mechanical gates | **verified-synthetic** | `tools/check_structure.py`, ctest `structure` | Enforces 500-line ownership and rejects generated/title dependencies in shared product code. |
| Kernel services | **absent** | capability `KernelServices` | Add only evidenced services with trapping unknown imports. |
| Graphics | **absent** | capability `Graphics` | No renderer or null renderer exists. |
| Audio | **absent** | capability `Audio` | No audio backend exists. |
| Input | **absent** | — | If added, one host device owner publishes neutral snapshots. |

## Source tree

```text
include/  —  322 lines, 4 files
└─ xenon_host/  322 lines, 4 files  # public GuestModule, GuestMemory, TitleAdapter, and Host interfaces
src/      —  783 lines, 7 files     # digest, guest-memory, validation, and composition implementations
tests/    —  551 lines, 2 files     # contract refusals plus real virtual-memory load/translation tests
tools/    —  105 lines, 1 file      # mechanical source-structure/dependency gate
docs/                # project coverage map
```

## Where is X?

- Exact module acceptance/refusal: `src/module_validation.cpp`, `ValidateModule`
- Exact import binding coverage: `src/module_validation.cpp`, `ValidateImports`
- Final pre-entry capability gate: `src/host.cpp`, `Host::Run`
- 4 GiB ABI window and bounded translation: `include/xenon_host/guest_memory.hpp`, `GuestMemory`
- Image load: `src/guest_memory.cpp`, `GuestMemoryLoader::Load`
- POSIX reservation and commit: `src/guest_memory_posix.cpp`, `GuestMemoryLoader::PlatformOperations`
- Opaque generated-code seam: `include/xenon_host/guest_module.hpp`, `GuestEntryThunk`
- Contract falsifier: `tests/contract_tests.cpp`
