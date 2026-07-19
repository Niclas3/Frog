# QEMU Automated Validation Loop

Date: 2026-07-18

## Problem

Frog currently treats QEMU as an interactive program. The build can be checked
noninteractively, but runtime validation still depends on a person watching VGA
output and stopping QEMU. This prevents an automated coding agent from reliably
distinguishing a successful boot from a hang, panic, triple fault, missing test
marker, or partially completed test run.

The solution is not an unbounded script that edits code by itself. Frog needs a
deterministic runtime test oracle. Once one command can build, boot, observe, and
classify a run, an agent can safely repeat this loop:

```text
edit -> build -> boot QEMU -> read structured result -> diagnose -> edit
```

## Decision

Implement the first automated runtime path as a small guest protocol plus a
bounded host runner. Keep it independent from the interactive QEMU and GDB
workflows.

### Guest Output Channel

Use QEMU's `isa-debugcon` device at I/O port `0xe9` for the first version.
Mirror kernel diagnostics to both VGA and debugcon so existing text-mode
debugging remains available. Debugcon is suitable for early boot and panic paths
because it only requires an `outb` and no UART initialization.

COM1 serial output remains a useful later logging backend, but it is not required
to establish the first QEMU-only smoke loop.

The output path must not suppress a kernel log merely because the current task
has a user page directory. In particular, a syscall executed by a ring-3 process
must be able to emit the ring-3 success marker.

### Guest Test Protocol

Emit stable, line-oriented records instead of asking the host to infer success
from ordinary diagnostic text. Keep the protocol usable before dynamic memory,
filesystems, or user space are available.

Example:

```text
FROGTEST v=1 BEGIN profile=boot-smoke
FROGTEST CASE mm.regression PASS
FROGTEST CASE frogfs.basic SKIP
FROGTEST MILESTONE ring3 PASS
FROGTEST END PASS
```

Kernel tests must return or accumulate explicit results. Printing `WARN` and
continuing is not sufficient because it can produce a false successful run.

### Guest Termination

Use QEMU's `isa-debug-exit` device on an explicitly configured I/O port. Under a
QEMU-test build option, the guest exits after the final result. Panic, assertion,
and explicit test failures must select a failure result. The normal interactive
kernel must not exit through this device.

The host runner must normalize the encoded `isa-debug-exit` process status rather
than treating its raw nonzero exit code as an ordinary command failure.

## Host Runner Contract

Provide one noninteractive entry point, initially:

```sh
./scripts/qemu-test.sh boot-smoke
```

The runner must:

1. Build every artifact used by the test from its real output path.
2. Create a unique temporary run directory.
3. Copy base disk images and mutate only disposable copies.
4. Start QEMU headlessly with no stdio monitor.
5. Capture debugcon output, QEMU stderr, and useful reset/guest-error logs.
6. Enforce a hard timeout.
7. Require both the expected final marker and the normalized guest exit result.
8. Terminate only the QEMU process started by that run; never use global `pkill`.
9. Preserve failure artifacts and produce a concise machine-readable result.

The result should classify at least:

```text
PASS
BUILD_FAILED
BOOT_TIMEOUT
PANIC
ASSERT_FAILED
TRIPLE_FAULT
EARLY_QEMU_EXIT
EXPECTED_MARKER_MISSING
TEST_FAILED
```

A host-side `result.json` may contain the classification, elapsed time, last
guest marker, normalized exit result, and artifact paths. Guest output itself
should remain simple line-oriented text so it is safe in early kernel code.

## Test Profiles

Start with separate profiles so fast, nondestructive validation remains the
default:

- `boot-smoke`: boot, memory regression, kernel initialization, ring-3 syscall
  marker, then exit. It must not write to a filesystem or disk test region.
- `process-smoke`: ring-3 fork, private-memory isolation, exit/wait status, and
  child-reaping checks without filesystem writes.
- `disk-smoke`: IDE and FrogFS round trips using disposable disk copies.
- `soak-10m`: bounded stability run with heartbeat/progress markers.
- Later graphical profiles: framebuffer rendering and input scenarios driven
  through QMP.

Each profile has an explicit timeout and expected ordered milestones. A compile
success or a single early marker never counts as a runtime pass.

## Graphical Validation

When framebuffer and compositor work begins, add a QMP Unix socket to the host
runner. QMP can request screenshots and inject keyboard or mouse events. Visual
validation should combine three sources:

1. Guest state markers, such as focused window and old/new coordinates.
2. Framebuffer checks, such as key pixels, region checksums, or reference-image
   comparison.
3. Saved screenshots for human or vision-model inspection.

Guest state is the primary functional evidence; screenshots are additional
evidence for blank output, corruption, overlap, and incorrect framing.

## Agent Loop Guardrails

The coding agent should invoke the runner after relevant changes and iterate only
from captured evidence. The orchestration layer must impose bounded attempts and
time limits, retain each failed run, and stop when the same unexplained failure
repeats rather than making speculative edits indefinitely.

Completion requires the declared profile to return `PASS`. Build-only validation
must be reported separately.

## Implementation Order

1. Add the port `0xe9` debugcon sink and correct the current printk filtering.
2. Add versioned `FROGTEST` markers and test result aggregation.
3. Add test-only `isa-debug-exit` pass/fail termination, including panic paths.
4. Implement a nondestructive, timeout-bounded `boot-smoke` host runner.
5. Repair the image packaging graph so the runner consumes freshly built files.
6. Move IDE and FrogFS writes into a disposable `disk-smoke` profile.
7. Add QMP screenshots and input injection with the framebuffer milestone.

This work implements the automated boot-smoke requirement in Milestone 0 and is
the prerequisite for reliable unattended development loops.
## Implementation status

The initial infrastructure is implemented by `scripts/qemu-test.sh`.
`boot-smoke`, `process-smoke`, and `disk-smoke` perform clean builds, package disposable image
copies, capture port `0xe9`, enforce a hard timeout, normalize the
`isa-debug-exit` status, and write `result.json`. The guest reports aggregate
case status and the ring-3 milestone; panic and architecture assertions use the
test-only failure exit path. Normal kernel builds do not enable debug-exit, and
destructive self-tests are compiled only for `disk-smoke`.
