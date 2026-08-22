---
id: 1
title: Exact-size guest image cannot satisfy Xenon generated memory ABI
status: resolved
symptom: generated PPC loads and indirect function lookup need base plus full 32-bit guest addresses, but shared host had no compatible memory owner
tags: xenon,guest-memory,abi
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

XenonRecomp addresses memory and its function table as `window_base + full_32_bit_guest_address`.
An exact-size allocation containing only the image therefore cannot supply a valid base for the
generated ABI.

## What was tried / dead ends

The first draft allocated only the exact image size. That made the image bytes available to native
code, but any generated access using an absolute guest address would form a pointer far beyond the
allocation. It was rejected before landing.

## Resolution

`GuestMemory` reserves one aligned 4 GiB virtual window, commits and loads the sealed image at its
guest address, exposes the window base to entry thunks, and checks native translations without
overflow. Injected host tests refuse reservation, alignment, and commit failures.
