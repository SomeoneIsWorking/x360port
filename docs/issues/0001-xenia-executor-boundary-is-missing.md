---
id: 1
title: Xenia executor boundary lacks device callbacks, overrides, and invalidation
status: open
symptom: x360port executes authenticated imported PPC but lacks the remaining title runtime boundaries
state_items: S005,S009,S010,S011,S012
tags: xenia,dynarec,executor
created: 2026-09-04
updated: 2026-09-08
---

## Root cause

The bounded Xenia owner, x64 JIT call, and typed function/variable import path
now exist. The remaining root cause is that override/original dispatch,
bounded non-returning execution, and executable
invalidation are not connected to Xenia's production owners. A real title
module therefore still cannot complete its runtime boundary.

## Progress note — 2026-09-08

`RuntimeContext::RegisterDeviceMemoryRange` now validates and owns masked
device ranges, registers them through Xenia `Memory::AddVirtualMappedRange`,
and reports read/write telemetry. The runtime discriminator executes both
synthetic PPC load and store instructions and checks callback values, addresses,
and counters. `RuntimeContext::NotifyExecutableWrite` now also validates a
title-reported PPC write range, removes affected cached Xenia functions, and
proves an unrelated function remains cached. Automatic write observation,
bounded exits, and internal guest-call routing remain open.

## Resolution condition

Build the real context, run validation against its parsed module, expose typed
imports/device callbacks/image-aware overrides/scoped original calls/bounded
exits/invalidation, and pass the Gears `0x8222E868` discriminator with nonzero
Xenia JIT work and no gameplay interpreter.
