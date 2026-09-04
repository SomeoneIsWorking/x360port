---
id: 2
title: Transfer validated contracts and retire the static host
status: open
symptom: authenticated image and typed import validation are stranded in a generated-code host that is not the target Xbox 360 runtime
state_items: S003,S004,S005
tags: x360port,xenia,migration,retirement
created: 2026-09-04
updated: 2026-09-04
---

## Root cause

`xenon-host` was designed around precomputed generated functions and a concrete
static PPC ABI. The portfolio now executes Xbox 360 code through Xenia's
existing dynarecs, so the reusable validation contracts and the obsolete static
contracts need different dispositions.

## Resolution condition

Move authenticated image and typed import validation into x360port and re-prove
both answers at its Xenia-backed boundary. Retire the generated function map,
entry ABI, static dispatcher, and ABI-only window instead of adapting them. Once
consumers move and no independent responsibility remains, remove this repository
and its references.
