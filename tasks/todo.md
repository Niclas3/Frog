# Poudland P0 Task Checklist

## Phase 0: Protect the baseline

- [x] Task 0: Re-establish current CI/process/framebuffer/input evidence and checkpoint explicit paths (`52eb687`).

## Phase 1: Kernel foundations

- [x] Task 1: Establish time64 monotonic and optional realtime ABI.
- [x] Task 2: Implement poll-shaped `wait2`.
- [x] Task 3: Connect input devices to common readiness.
- [x] Checkpoint A: All baseline, time, wait2, and input tests pass.

## Phase 2: Runtime and transport

- [x] Task 4: Add eager private anonymous mappings.
- [x] Task 5: Implement user-space `malloc/free`.
- [x] Task 6: Implement packagefs bind/connect/directed records.
- [x] Task 7: Complete packagefs lifecycle/readiness.
- [x] Checkpoint B: Anonymous memory, allocator, and packagefs pass under 16 MiB.

## Phase 3: Independent vertical slices

- [x] Task 8: Build the pointer-free packagefs user library.
- [x] Task 9: Implement the Poudland Version 1 client runtime.
- [x] Task 10: Establish the modern built-in compositor slice.
- [x] Task 11: Create the deterministic FrogFS host image builder.
- [x] Checkpoint C: Protocol fixture, built-in compositor, and generated FrogFS image each work independently.

## Phase 4: Client desktop path

- [ ] Task 12: Produce installable compositor and desktop ELFs.
- [ ] Task 13: Connect HELLO/create/close end to end.
- [ ] Task 14: Complete pointer, keyboard, focus, drag, and damage behavior.
- [ ] Task 15: Launch and supervise graphical children from user init.
- [ ] Checkpoint D: Normal boot loads both ELFs and completes the fixed interaction scenario.

## Phase 5: Automated acceptance

- [ ] Task 16: Add guest desktop-smoke state reporting.
- [ ] Task 17: Add the 16 MiB host desktop-smoke profile.
- [ ] Task 18: Add the separate ten-minute desktop soak.
- [ ] Final checkpoint: All existing regressions, desktop-smoke, soak, screenshot, and cleanup evidence pass.

## Deferred after P0

- [ ] Stabilize FrogFS as the production root and migrate `/test` paths.
- [ ] Design shared Window Surface mapping/commit/damage.
- [ ] Migrate useful non-P0 legacy compositor features.
