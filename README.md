# x360port

`x360port` is the shared Xbox 360 runtime boundary around Xenia. Xenia owns
Xenon decoding, its x64/A64 dynamic recompilers, executable memory, guest
memory, and translated-block caching. This package owns only the narrow
embedding contracts shared by title ports.

The first landed slice is fail-closed authenticated-module and typed-import
validation. It preserves independently falsified image/layout and canonical
function/variable import rules while the actual Xenia executor is still
missing. The CMake target is therefore named `x360port_validation`; there is no
`x360port` executor target, executable, interpreter, or static-code bridge.

The next milestone must embed Xenia `Memory`, `Processor`, `ThreadState`, and
`RawModule`, then exercise these validators on that production boundary. Gears
1 is the first consumer discriminator: execute leaf `0x8222E868`, bind typed
`DbgPrint`, and prove disabled, enabled, and scoped-original override calls
through Xenia with nonzero JIT work.

Title addresses, identities, imports, overrides, and policies remain in their
title repositories. No generated function map, generated entry ABI, standalone
guest-memory window, or static dispatcher belongs here.
