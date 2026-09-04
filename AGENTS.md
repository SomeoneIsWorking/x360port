# xenon-host retirement guidance

Read `../jit-common/docs/migration.md`, `docs/project-state.md`,
`docs/migration.md`, and `docs/codemap.md` before changing this repository.

`xenon-host` is not the target Xbox 360 framework. The target is
`shared/x360port`, a narrow wrapper around Xenia's existing x64/A64 Xenon
dynarecs. Xenia owns PPC decoding, lowering, host-code emission, executable
memory, and the translated-block cache. Do not add an interpreter, a new Xenon
CPU runtime, or `jit-common` cache ownership here.

This repository is a migration source only:

- move its validated authenticated-image and typed import validation contracts
  into `x360port`, adapting them to Xenia `Memory`, `Processor`, `ThreadState`,
  and `RawModule`;
- do not migrate the address-only generated function map, generated entry ABI,
  static dispatch, or 4 GiB window solely required by generated code;
- preserve the current synthetic evidence until equivalent x360port positive
  and negative tests exist; and
- remove this separate repository after the transfer if no independent owner
  remains. Do not leave a compatibility library, tombstone, or second authority.

No new consumer may integrate this API. Do not extend its source, tests, or
tools for the former static product. Documentation changes may clarify the
transfer and retirement only. Current dirty implementation work must be
preserved until the operator decides how it contributes to the transfer.

The first consuming discriminator belongs to Gears, not here: execute real leaf
`0x8222E868`, call `DbgPrint` through a typed import, and prove disabled,
enabled, and scoped-`super` override paths through Xenia. Representative
interactive gameplay is the later completion gate; Gears deletes its old
static path before implementing this discriminator.
