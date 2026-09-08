# Project state

## Comparison baseline

The baseline is direct per-title Xenia integration or the retired generated-code
host approach. The target is one reusable Xenia-backed runtime boundary with no
static or interpreter product path.

| ID | Capability | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | Authenticated image/layout validator is synthetically falsified | verified | — | G002 |
| S002 | Typed import manifest/binding validator is synthetically falsified | verified | — | G002 |
| S003 | Xenia runtime objects have one bounded embedding context | verified | — | G001 |
| S004 | Retained validators execute against Xenia `RawModule` | verified | S001, S002, S003 | G001, G002 |
| S005 | Gears leaf/import/override discriminator executes through Xenia | missing | S003, S004 | G001 |
| S006 | Xenia A64 execution is qualified on Apple Silicon macOS | missing | S003, S004 | G001 |
| S007 | Xenia A64 execution is qualified on Android arm64-v8a | missing | S003, S004 | G001 |
| S008 | Typed function and variable imports execute through Xenia exports | verified | S002, S003, S004 | G001, G002 |
| S009 | Device-memory callbacks have a title-neutral runtime boundary | verified | S003, S004 | G001 |
| S010 | Image-aware overrides and scoped original calls use Xenia dispatch | partial | S003, S004, S008 | G001 |
| S011 | Guest calls have bounded exit and refusal contracts | partial | S003, S004 | G001 |
| S012 | Executable writes invalidate Xenia translations coherently | partial | S003, S004 | G001 |
| S013 | Xenia x64 dynarec executes authenticated PPC and reuses host code | verified | S003, S004 | G001 |
| S014 | Asset-free native-host runtime CI executes the synthetic JIT contract | partial | S003, S004, S008, S013 | G001 |
| S015 | Checked XEX2 inspection and canonical normalized-image output | verified | S001, S002, S003, S004 | G001, G002 |

## Current focus

S012 is the current focus. The real `x360port` target executes authenticated PPC
through Xenia's x64 dynarec and its typed function and variable imports cross
Xenia's production export machinery. The device-backed guest-memory callback
boundary is now proven; executable-write notification and Xenia virtual-memory
observation now invalidate affected cached functions while preserving unrelated
entries. The next shared gap is internal guest-call routing and bounded exit
behavior.

## Capability details

### S001 — image validation

Evidence: `x360port_contract_tests` carries independent SHA-256 known answers,
the shared PE-to-flat-image positive discriminator, and malformed source/geometry
refusals. The retained module validator still mutates every authenticated
image/layout field, including a malformed image base that must receive a typed
refusal before Xenia fixed allocation. `x360port::MapPeImage` preserves the
normalized source digest separately from the flat runtime-image digest.

### S002 — import validation

Evidence: the same production validator test covers accepted function and
variable imports, every sealed manifest field, every named refusal, and wrong
callback-shape controls.

### S003 — Xenia context

Evidence: `RuntimeContext` owns Xenia `Memory`, `Processor`, `ThreadState`, and
the registered `RawModule`, enforces the process-global fixed mapping as one
active instance, and releases stack/image mappings after their Xenia owners.
The teardown/recreation test reloads and executes from the same guest range.

### S004 — Xenia-backed validation

Evidence: `RuntimeContext::LoadModule` runs the retained module/import validators
before `AllocFixed` and `RawModule::SetAddressRange`. A valid synthetic image is
then registered as the production Xenia `RawModule`; invalid identity and an
unresolvable variable import fail before guest memory is committed.

### S005 — Gears discriminator

Missing capability: execute the authenticated Gears leaf/import/override
round-trip through Xenia with nonzero dynarec work and no interpreter link.

### S006 — Apple Silicon macOS

Missing capability: qualify Xenia's A64 dynarec on Apple Silicon macOS,
including executable-memory protection, instruction-cache coherence, host ABI,
exception behavior, and representative gameplay.

### S007 — Android arm64-v8a

Missing capability: qualify Xenia's A64 dynarec in the Android arm64-v8a
product package, including executable-memory protection, instruction-cache
coherence, host ABI, signal behavior, sustained execution, and representative
gameplay.

### S008 — typed runtime imports

Evidence: `x360port_runtime_tests` loads an authenticated synthetic module whose
guest PPC calls a function import and loads a variable import. The function
crosses Xenia's real syscall thunk and typed export callback, the variable is
published through Xenia's resolver into the guest record, and both return
independently checked values. A null variable resolution fails before allocation
and the same context then loads successfully; callback tables remain valid after
the caller's manifest and binding objects leave scope.

### S009 — device-memory callbacks

Evidence: `RuntimeContext::RegisterDeviceMemoryRange` validates and registers a
title-neutral masked range through Xenia `Memory::AddVirtualMappedRange`. The
runtime test executes synthetic PPC `lwz` and `stw` instructions, checks both
callback values and addresses, and proves read/write counters. The callbacks
remain owned by the runtime context rather than by title or engine policy.

### S010 — overrides and original calls

Evidence: `RuntimeContext::InstallOverride` and `CallOriginal` dispatch a
validated guest entry through a title-owned native handler, let that handler
re-enter the same guest address through Xenia without recursion, and invalidate
the address entry when the override is installed or removed. The runtime test
proves native result wrapping, original-call and invalidation counters, and
restored dynarec execution.

Gap: this is only the public entry-dispatch contract. A real Gears image must
exercise the authenticated leaf and prove internal guest call paths, executable
writes, and title override bindings invalidate all affected Xenia translations.

### S011 — bounded calls

Evidence: entry addresses are constrained to authenticated code, the call ABI
accepts at most eight register arguments, and LR/SP are restored through an
exception-safe call frame. Gap: a runtime-owned execution budget or
cancellation/exit contract for guest code that does not return.

### S012 — executable invalidation

Evidence: `RuntimeContext::NotifyExecutableWrite` validates a non-empty range
inside the authenticated code range, finds every cached Xenia function touched
by the aligned PPC write range, and removes each function once. Xenia's maintained
virtual-memory watch API now traps writes to the authenticated virtual code range;
the runtime drains the coalesced range at the next guest-call boundary, resets the
affected module functions to declared state, and re-translates them from the
modified guest bytes. The runtime test proves the automatic path with a PPC
self-modifying leaf, including nonzero observation and the changed return value,
while preserving unrelated translations. Remaining gap: writes made and then
executed before the guest returns are not drained mid-call; internal guest-call
routing must own that boundary before title behavior can rely on it.

### S013 — x64 JIT execution

Evidence: `x360port_runtime_tests` loads big-endian PPC `li r3, 42; blr`, requires
Xenia to emit a non-empty host function, executes it twice, observes return 42,
and proves the second call does not increment the translation count. The same
test tears down and recreates the runtime before reloading the identical range.

### S014 — native-host runtime CI

Evidence: the locked Python verifier is the only CI entry point and requires the
real synthetic Xenia runtime tests on a declared host architecture. Linux
x86-64 is locally green. The pinned workflow definitions cover Linux x86-64,
Windows x86-64, and Apple Silicon macOS, but their remote runs remain unverified
until a green hosted run confirms them. Run `33894756332` exposed a missing Linux
Xlib/XCB development header, MSVC-only `/MP` passed to clang-cl, and an unprobed
macOS warning option. The pinned fork fixes compiler-family/capability
selection, the verifier selects Xcode AppleClang explicitly, and Linux provisioning
includes `libx11-xcb-dev`.
[Windows job 101286402490](https://github.com/SomeoneIsWorking/x360port/actions/runs/33958625790/job/101286402490)
then exposed a `constexpr HANDLE` initialized with `INVALID_HANDLE_VALUE`, whose
integer-to-pointer conversion is forbidden in a C++ constant expression. The
pinned fork now keeps the sentinel immutable with `inline const`. A Clang Windows
target accepts the actual production declaration; restoring `constexpr` in the
negative control reproduces the hosted failure.
[Windows job 101287961139](https://github.com/SomeoneIsWorking/x360port/actions/runs/33959201949/job/101287961139)
then rejected an impossible null-address check on `Win32Thread::set_name`'s
by-value string parameter. Removing that guard preserves the platform and
metadata naming calls; the actual production method compiles with warnings as
errors, while restoring the guard reproduces the hosted diagnostic.
[Windows job 101289741182](https://github.com/SomeoneIsWorking/x360port/actions/runs/33959862352/job/101289741182)
then exposed incomplete builtin/ISA configuration in Xenia's duplicate zlib-ng
build. Xenia now consumes the pinned library's CMake target: that owner probes
compiler capabilities, selects SIMD sources and per-file flags, and exports
generated headers from the binary directory. The XLast consumer uses that header
interface. The rebuilt native graph passes the runtime suite and a linked
compression roundtrip/malformed-input discriminator. The actual x86 header and
production target definitions compile under clang-cl; removing the builtin
capabilities reproduces both undeclared optimized sliders. Full Windows runtime
execution still awaits hosted verification. Android is explicitly not
represented by a placeholder job: no Android executable/package owner exists to
run this contract on arm64-v8a yet.

The next collected hosted failures exposed clang-cl interpreting bare `-Wall`
as MSVC `/Wall`, incomplete enum-subset switches, implicit Windows function-pointer
conversions, nontrivial guest-record copies, and detached `jthread` usage unavailable
in the macOS library. The maintained fork and native-driver warning owner address
those boundaries without disabling diagnostics. The focused Xenia suite preserves
guest/texture-key bytes and instruction-key encodings and requires impossible HIR
types to abort in both Debug and Release. It belongs only to top-level framework
verification; consuming games do not build this test harness.

Local evidence: the combined Clang build compiles all 240 affected steps, 10/10
Python tests pass, and 23/25 CTests pass initially. The two fatal probes expose a
test-discovery newline mismatch in Xenia's console entry; after correcting only
that parser boundary, both shipping-executable probes and the harness controls
pass (3/3 focused CTests). All 25 registered checks are covered. A subsequent
reconfigure/build performs zero compilations. Existing untouched upstream lint
findings remain; this is not a warning-clean whole-Xenia claim.

Gap: Windows/macOS qualification still requires hosted confirmation. The separate
tabulate literal-operator correction is not included or pinned: publishing its
maintained fork is paused for the user's upstream-history decision. No Windows
success is inferred while that known dependency failure remains.

### S015 — checked XEX2 inspection

Evidence: `x360-xex-inspect` validates XEX2 header, security geometry, file-format
and payload bounds before entering Xenia, then uses the pinned Xenia loader's
decryption/decompression path, canonicalizes import records and function stubs,
maps the PE, and reports execution metadata, ordered logical imports, and the
eight register save/restore helper scans. The real ignored Gears 1 XEX produces
the existing checked authority's 13,500,416-byte image with SHA-256
`f61cc78e4057bc68a2c65386a0341f6d26a7add3dfd9918007a455750ec6ed5c`, 17
sections, 236 imports, and one hit for each helper pattern. Focused negative
tests reject truncated, non-XEX2, payload-less, window-less, and AES-misaligned
inputs. The pinned Xenia fork also bounds basic/normal-compression reads before
copying or decompressing untrusted blocks.

Gap: the inspector is a shared loading contract, not yet the complete Gears
title adapter; authenticated image binding, runtime services, and the real leaf
round-trip remain in S005.
