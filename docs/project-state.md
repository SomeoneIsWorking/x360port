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
| S009 | Device-memory callbacks have a title-neutral runtime boundary | missing | S003, S004 | G001 |
| S010 | Image-aware overrides and scoped original calls use Xenia dispatch | missing | S003, S004, S008 | G001 |
| S011 | Guest calls have bounded exit and refusal contracts | partial | S003, S004 | G001 |
| S012 | Executable writes invalidate Xenia translations coherently | missing | S003, S004 | G001 |
| S013 | Xenia x64 dynarec executes authenticated PPC and reuses host code | verified | S003, S004 | G001 |
| S014 | Asset-free native-host runtime CI executes the synthetic JIT contract | partial | S003, S004, S008, S013 | G001 |

## Current focus

S009 is the current focus. The real `x360port` target executes authenticated PPC
through Xenia's x64 dynarec and its typed function and variable imports cross
Xenia's production export machinery. The next shared runtime gap is a narrow
device-backed guest-memory callback boundary.

## Capability details

### S001 — image validation

Evidence: `x360port_contract_tests` carries independent SHA-256 known answers
and mutations of every authenticated image/layout field, including a malformed
image base that must receive a typed refusal before Xenia fixed allocation.

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

Missing capability: define and execute a narrow title-neutral callback contract
for device-backed guest ranges without importing title or engine policy.

### S010 — overrides and original calls

Missing capability: dispatch image-authenticated overrides through Xenia and
scope an original call so it suppresses exactly one matching override.

### S011 — bounded calls

Evidence: entry addresses are constrained to authenticated code, the call ABI
accepts at most eight register arguments, and LR/SP are restored through an
exception-safe call frame. Missing capability: a runtime-owned execution budget
or cancellation/exit contract for guest code that does not return.

### S012 — executable invalidation

Missing capability: prove executable guest writes invalidate affected Xenia
translations and leave unrelated cached functions intact.

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
