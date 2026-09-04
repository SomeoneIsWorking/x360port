---
id: 1
title: Xenia executor boundary is missing
status: open
symptom: x360port validates synthetic module contracts but cannot load or execute guest code
state_items: S003,S004,S005
tags: xenia,dynarec,executor
created: 2026-09-04
updated: 2026-09-04
---

## Root cause

Only the reusable validation slice has moved. No bounded owner yet composes
Xenia `Memory`, `Processor`, `ThreadState`, and `RawModule`, and process-global
memory/MMIO/clock assumptions have not been made explicit.

## Resolution condition

Build the real context, run validation against its parsed module, expose typed
imports/device callbacks/image-aware overrides/scoped original calls/bounded
exits/invalidation, and pass the Gears `0x8222E868` discriminator with nonzero
Xenia JIT work and no gameplay interpreter.
