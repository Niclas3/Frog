# Poudland P0 Final Handoff

Status: Complete on 2026-08-09

This is the durable handoff for the Poudland P0 and `desktop.c` milestone. The
implementation is complete through commit `0363fc4` on branch
`refine/code_arch`; the final documentation commit follows this checkpoint.
The only unrelated working-tree change at handoff is the owner's existing
`booter/Makefile` edit. It was never edited, staged, or committed by this work.

## Delivered Outcome

Frog now boots a graphical ring-3 init under 16 MiB, mounts a reusable FrogFS
disk at `/test`, forks and executes `/test/compositor` and `/test/desktop`, and
supervises both children without depending on their exit order. The real
`desktop.c` client connects to the compositor through packagefs and Poudland
Version 1, creates three windows, closes the third, and leaves two visible
client-owned windows.

The automated interaction presses the left button over window 2, sends the
`a` key, moves the pointer by `(40,25)`, and releases the button. The compositor
focuses window 2, routes the key to that client, moves the window from
`(220,200)` to `(260,225)`, moves the cursor from `(230,210)` to `(270,235)`,
and redraws only damaged regions. The host then verifies every pixel of the
1024x768 scene, including the focused white border, alpha-blended cursor, two
remaining windows, and absence of the third window.

This completes the accepted P0 goal. It is a minimal server-rendered desktop
path, not yet a complete desktop environment or a shared client-surface API.

## Implemented Foundations

The completed chain is:

```text
8254/PIT monotonic time
    -> level-triggered wait2/poll
    -> /dev/input/event0 and /dev/input/event1 readiness
    -> eager anonymous mmap and mmap-backed user malloc/free
    -> refcounted record-oriented packagefs
    -> Poudland Version 1 client/server protocol
    -> damage-aware compositor and framebuffer backbuffer
    -> graphical init fork/exec supervision
    -> desktop-smoke and desktop-soak-10m
```

The work delivered the following contracts.

- Timekeeping uses PIT channel 0 as the required monotonic interval source.
  `clock_gettime(CLOCK_MONOTONIC)` is always available after initialization;
  realtime and `gettimeofday` use RTC only when a valid RTC origin exists and
  otherwise return `-ENODATA`. Time values use signed 64-bit ABI fields, which
  GCC implements correctly on i386 without requiring 64-bit hardware data or
  address buses.
- `wait2` is a small pollfd-shaped, millisecond-timeout, level-triggered API.
  It clears and returns `revents`, handles invalid/closed fds, supports several
  ready sources in one call, and is used by the compositor event loop.
- Keyboard and mouse are independent byte/packet event sources at
  `/dev/input/event0` and `/dev/input/event1`. `/dev/tty0` is not required by
  Poudland. The common readiness mechanism can accept future event sources.
- Anonymous mappings are eager, private, zero-filled, and fully rolled back on
  partial failures. The user allocator is built over those mappings; fork,
  exec, unmap, and exit lifetimes have focused tests.
- Packagefs exposes `/dev/pkg/<service>` with one named server, opaque client
  IDs, complete bounded records, one C2S queue, one S2C queue per client,
  blocking/nonblocking I/O, level readiness, backpressure, directed replies,
  disconnect records, and final-reference teardown. Abrupt process exit and
  explicit close use the same refcounted path; correctness does not depend on
  a cooperative TCP-style close handshake. The accepted design is recorded in
  `doc/packagefs-design.md`.
- Poudland Version 1 uses fixed-width pointer-free messages, request IDs,
  explicit version and protocol errors, session-owned window IDs, ownership
  checks, bounded sessions/windows, and disconnect cleanup. Reliable replies
  retain FIFO order; replaceable motion/configure state may coalesce without
  overtaking reliable transitions.
- The compositor is a normal i386 user ELF. It maps `/dev/fb0`, reads VBE mode
  information, loads `/test/b.bmp`, opens both input event fds, binds the
  `compositor` packagefs service, and runs one `wait2`-driven event loop. It
  keeps one display-sized backbuffer, not a full buffer per window, and copies
  only damaged rectangles to the physical framebuffer.
- `desktop.c` is a normal Poudland client ELF. It performs HELLO/WELCOME,
  creates three solid-color windows, validates the returned IDs and geometry,
  closes the third with confirmation, and continues consuming pointer,
  keyboard, and configure events for the two live windows.
- Graphical PID 1 mounts the second disk's FrogFS partition, starts the
  compositor and desktop with `fork -> execv`, reaps either order, reports
  bounded failure statuses, and then remains alive. No alternate product ELF
  is used for the acceptance path; test reporting is compiled only into the
  disposable test copies.

The detailed public contracts remain in:

- `doc/timekeeping-design.md`
- `doc/wait2-design.md`
- `doc/mmap-design.md`
- `doc/user-runtime-memory-design.md`
- `doc/packagefs-design.md`
- `doc/poudland-protocol-v1.md`
- `doc/graphical-startup-image-design.md`
- `doc/frogfs-host-image.md`
- `doc/qemu-automated-validation.md`

## Reusable FrogFS Images

Normal builds generate `build/frog-root.img` directly from
`config/frog-root.manifest`. It contains `/compositor`, `/desktop`, and
`/b.bmp` in a FrogFS partition and is attached as the second IDE disk. This is
the reusable disk requested for normal startup; it does not require a guest
prepare phase on every boot.

`desktop-smoke` and `desktop-soak-10m` generate the deterministic
`build/desktop-smoke-root.img`. It contains instrumented copies built from the
same application sources and uses the same paths. Every QEMU run attaches a
unique copy and checks that the base SHA-256 is unchanged. The fast and soak
profiles produced the identical base hash:

```text
bf0bce30b496cc32e75838063d3d1c454d86fe53e6ef03c9fe473770aa43741e
```

The last normal production artifacts were:

```text
23760  core/apps/build/compositor
13912  core/apps/build/desktop

9835bc70a67aebffa2a48a0c92d749d18dafbb8e47ebe8949eeb9f8035695d4b  compositor
8b55d650f779174adde428e95bccb7f333866f1d49be267067334c2ab662864f  desktop
df207621186911d2be7796c5b8158dbd7684f46681682fe841d22a682cf097b4  frog-root.img
```

There is no 4 KiB process-size limit. Both production ELFs are deliberately
larger than 4096 bytes and execute from FrogFS. The ELF loader instead enforces
bounded program-header and mapped-page counts; the separately embedded
graphical-init bootstrap is small by design. The boot image has its own
independent 512-sector (256 KiB) kernel payload reservation.

## How to Build and Run

For the normal interactive QEMU path:

```sh
cd /home/zm/Development/C/Frog/src
make frog-root-verify
make run
```

`make run` rebuilds the normal 16 MiB boot disk and reusable FrogFS root disk,
then launches QEMU with VGA output. The graphical init starts both installed
ELFs automatically. QEMU debugcon output is written to
`build/frog-run-debugcon.log`.

For the fast noninteractive acceptance path:

```sh
./scripts/qemu-test.sh desktop-smoke
cat build/qemu-test/desktop-smoke-result.json
```

For the separately invoked ten-minute stability path:

```sh
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh desktop-soak-10m
cat build/qemu-test/desktop-soak-10m-result.json
```

QMP profiles must be run serially because the build stage cleans shared kernel
artifacts. A restricted sandbox may deny the private Unix QMP socket; that is
an environment capability failure, not automatically a guest regression.

## Final Validation Evidence

All of the following passed from commit `0363fc4` or from the exact staged code
committed there:

| Check | Result | Important evidence |
| --- | --- | --- |
| `make -C tools test` | PASS | FrogFS host builder, corruption and reuse cases |
| `make -C core/apps test` | PASS | production ELF and host protocol/compositor tests |
| `make frog-root-test` / `make frog-root-verify` | PASS | deterministic normal root image |
| `./scripts/CI.sh` | PASS | full compile and artifact graph, rerun after QEMU matrix |
| `process-smoke` | PASS | `2026-08-09T08:17:13Z`, process/VM lifecycle |
| `disk-smoke` | PASS | `2026-08-09T08:17:19Z`, all three disk stages; corruption copy unchanged |
| `framebuffer-mmap-smoke` | PASS | `2026-08-09T08:17:51Z`, 1024x768 exact frame and mmap cleanup |
| `input-smoke` | PASS | `2026-08-09T08:17:57Z`, keyboard/mouse QMP injection and wait2 readiness |
| `packagefs-lifecycle-smoke` at 16 MiB | PASS | `2026-08-09T08:18:37Z`, final-reference cleanup |
| `desktop-smoke` at 16 MiB | PASS | `2026-08-09T08:18:18Z`, ordered guest state and every-pixel final scene |
| `desktop-soak-10m` at 16 MiB | PASS | 10 heartbeats, 600 seconds, stable resources and exact final frame |

The canonical soak result is
`build/qemu-test/desktop-soak-10m-result.json`. It records
`soak_heartbeat_count: 10`, `soak_duration_seconds: 600`,
`soak_resources_stable: true`, 1024x768, and equal base hashes before/after.
The retained positive artifact directory is:

```text
build/qemu-test/20260809T081402Z-desktop-soak-10m-PASS-383085/
```

Its debugcon log contains exactly one ordered heartbeat for every minute,
followed by:

```text
FROGTEST CASE desktop-soak.state-stable PASS
FROGTEST CASE desktop-soak.resources PASS
FROGTEST SYNC desktop-soak-complete
```

The watchdog failure injection was also exercised. It reached the complete
desktop scene and `desktop-soak-start`, then deliberately failed the host wait
after two seconds. It was classified as `EXPECTED_MARKER_MISSING` and retained
at:

```text
build/qemu-test/20260809T080037Z-desktop-soak-10m-EXPECTED_MARKER_MISSING-381043/
```

Earlier desktop negative evidence also proved distinct classifications for a
wrong drag coordinate, a missing guest case, a wrong pixel, and a stale normal
root image. Failed runs retain logs, QMP transcripts, screenshots when
available, private disks, and result JSON.

## Commit Checkpoints

```text
52eb687 feat(input): expose ring3 ps2 device events
2a41606 docs(poudland): define P0 runtime contracts
d0bc8f2 docs(plan): stage Poudland P0 implementation
3aeba97 feat(time): add PIT-backed time64 clock
51db77d feat(wait): add level-triggered wait2 poll
eb193d3 fix(input): expose mouse readiness to wait2
006bcbb feat(vm): add eager anonymous mappings
ff24b45 feat(runtime): add mmap-backed user allocator
35b2e24 feat(packagefs): add record-oriented transport
7bf1e37 feat(packagefs): complete endpoint lifecycle
2f69396 feat(packagefs): add pointer-free user transport
c9d82cd feat(poudland): add version 1 client runtime
cc2940c feat(poudland): add built-in compositor slice
0e8724f feat(frogfs): add deterministic host image builder
822ca01 feat(apps): install compositor and desktop ELFs
0679477 feat(poudland): connect desktop end to end
972445d feat(poudland): route production input events
5865d19 feat(init): supervise graphical desktop
7ac551b test(qemu): validate graphical desktop
0363fc4 test(qemu): add desktop soak validation
```

## Known Limits and Next Work

The accepted P0 defers these items; none is required to reproduce the result
above.

- Stabilize FrogFS as the production root and migrate `/test/compositor`,
  `/test/desktop`, and `/test/b.bmp` to final root paths such as `/bin` and a
  shared-data directory. The current generated disk is already reusable; this
  work is about root namespace and long-term filesystem policy.
- Design shared Window Surface allocation, mapping, attach, commit, damage,
  lifetime, and failure recovery. Packagefs should remain the small control
  plane rather than carry bulk pixels.
- Add resize, decorations, clipping beyond P0, clipboard, richer keyboard
  events, more clients, compositor restart policy, and useful legacy Poudland
  features.
- Add an edge-triggered readiness mode only after the existing simple
  level-triggered `wait2` contract remains stable.
- Confirm RTC hardware and SVGA controller details on the real board; realtime
  remains optional and compositor scheduling uses monotonic PIT time.

## First Physical Hardware Target

The first required machine remains the 40 MHz 386SX-compatible M6117 system
with 16 MiB RAM, 1 GB CompactFlash, RS-232, DB25 parallel/Covox, a 16-bit ISA
expansion bus, RTL8019AS network, Yamaha YMF262-M audio plus Covox, and SVGA.
The software baseline assumes no CPUID, TSC, APIC, HPET, ACPI, or SMP. It uses
8259 IRQ routing and the 8254/PIT baseline; RTC is optional until the exact
board clock, battery, and wiring are confirmed. See `doc/hardware-targets.md`
for bring-up order and the still-required board measurements.

## Repository State at Handoff

Generated images, binaries, screenshots, and QEMU artifacts are intentionally
untracked. The final source tree should show only:

```text
 M booter/Makefile
```

That file is the owner's unrelated change and must remain untouched. To resume
future work, start with `git status --short`, read this document plus the
specific design document for the next task, and rerun the relevant focused
profile before editing.
