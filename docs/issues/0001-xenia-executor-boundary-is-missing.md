---
id: 1
title: Xenia executor boundary lacks device callbacks, overrides, and invalidation
status: open
symptom: x360port executes authenticated imported PPC but lacks the remaining title runtime boundaries
state_items: S005,S009,S010,S011,S012
tags: xenia,dynarec,executor
created: 2026-09-04
updated: 2026-09-12
---

## Root cause

The bounded Xenia owner, x64 JIT call, typed function/variable import path, and
finite translated-block exit contract now exist. The remaining root cause is
that override/original dispatch, reason-labelled interpreter fallback, and
complete title-level executable invalidation semantics are not fully connected
to Xenia's production owners.
A real title
module therefore still cannot complete its runtime boundary.

## Progress note — 2026-09-12

`RuntimeContext::RegisterDeviceMemoryRange` now validates and owns masked
device ranges, registers them through Xenia `Memory::AddVirtualMappedRange`,
and reports read/write telemetry. The runtime discriminator executes both
synthetic PPC load and store instructions and checks callback values, addresses,
and counters. `RuntimeContext::NotifyExecutableWrite` now also validates a
title-reported PPC write range, removes affected cached Xenia functions, and
proves an unrelated function remains cached. Automatic write observation,
bounded exits, ordinary internal guest calls, and mid-call invalidation are now
proven in the synthetic runtime. Function imports now also expose register
arguments, return propagation, and bounded guest-memory access through the
title-neutral `GuestImportContext` while retaining Xenia's private trampoline
ABI. The active call exits with
`ExecutionInvalidated` after the watched guest store, and the next guest entry
drains the pending range before dispatch. Reason-labelled interpreter fallback
and real-image invalidation paths remain open. The checked XEX inspector now
resolves the XEX import-library string table by library index and alignment,
preserving the real image's `xam.xex` and `xboxkrnl.exe` bindings.

## Native override falsifier — 2026-09-12

A temporary discriminator in `tests/runtime_tests.cpp` installed the native
`AddOneThroughOriginal` handler on the existing synthetic internal callee at
`kCodeAddress + 96`. Direct `RuntimeContext::Execute` returned 18 from the
original value 17, proving the handler and scoped original were live. The
already-translated guest caller returned 17; a caller first translated after
installation also failed to return the wrapped value 18. The latter failed
with `a newly translated guest caller did not use the native override`.
The probe was removed after measurement; the shipping test suite remains green
but does not prove internal override dispatch.

`RuntimeContext::Execute` alone checks `OverrideDispatch::Find`; translated
guest calls use Xenia's code-cache path. The pinned fork now routes both direct
and indirect calls through guest-address indirection entries and resets an
invalidated entry to the resolve thunk. The remaining correction is for
`x360port` to install an image-scoped native target into that entry, expose the
full PPC/guest-memory ABI, preserve a separately callable original body, and
restore the ordinary target on removal. A host-entry-only wrapper is not a
native game override.

## Resolution condition

Build the real context, run validation against its parsed module, expose typed
imports/device callbacks/image-aware overrides/scoped original calls/bounded
exits/invalidation, and pass the Gears `0x8222E868` discriminator with nonzero
Xenia JIT work and no gameplay interpreter.
