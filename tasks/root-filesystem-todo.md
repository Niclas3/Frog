# Production Root Filesystem Checklist

Status: Complete. Task 5.3 and the final checkpoint passed on 2026-08-10. The
implementation remains uncommitted, so no commit SHA is claimed.

This checklist mirrors `tasks/root-filesystem-plan.md`. Check an item only with
the verification evidence required by that plan; compile-only evidence is not
a runtime pass.

## Phase 0: Documentation and Baseline

- [x] Accept the production directory and image-role contract.
- [x] Record ADRs for the System Image, init selection, Root Switch, and Root Locator.
- [x] Write the phased implementation and validation plan.
- [x] Add baseline document links without rewriting Poudland P0 history.

## Phase 1: Bootstrap Root Naming

- [x] Rename the directory-only `tmpfs` backend to `rootfs` without behavior changes.
- [x] Pass compile and the existing focused filesystem regression.

## Phase 2: Deterministic System Image

- [x] Add manifest Version 2 `volume` and explicit `dir` records.
- [x] Cover parent, conflict, label, ordering, reuse, corruption, and atomic-publication host cases.
- [x] Define and host-verify the strict `mode=graphical` input.
- [x] Define the exact production manifest and package the strict init input.
- [x] Add a non-overriding test overlay and remove test-only programs from production contents.
- [x] Checkpoint A: verify deterministic `frog-root` image hashes and contents.

## Phase 3: Root Namespace

- [x] Add bounded block-device enumeration without exposing the registry list.
- [x] Select exactly one valid `frog-root`; classify zero, duplicate, corrupt, and unreadable cases.
- [x] Add the narrow one-time Root Switch with explicit ownership and rollback.
- [x] Preserve the existing devfs and nested packagefs mount subtree at `/dev`.
- [x] Split FrogFS registration from `/test` and production mount policy.
- [x] Mount `/sysroot` read-only and enforce `-EROFS` on mutation.
- [x] Keep disposable writable FrogFS tests mounted at `/test`.
- [x] Checkpoint B: prove the switched root and all required devices at runtime.

## Phase 4: Disk-loaded Init and Production Paths

Standalone `init.elf` and `init-graphical.elf`, strict configuration parsing,
and installed graphical child paths are verified and packaged in the
production image. Task 4.3 proves exact production System Init and Graphical
Init execution, child supervision and reaping, status output, and the PID1
stop loop through the real read-only root chain. Focused child stubs and
explicit legacy P0 aliases remain test-only; exact production application ELF
loading is proved separately.

- [x] Reuse the ELF loader to create PID1 from `/sbin/init`.
- [x] Add strict System Init parsing and exec `/sbin/init-graphical` as PID1.
- [x] Package Graphical Init while preserving Poudland P0 supervision behavior.
- [x] Migrate programs to `/bin/compositor` and `/bin/desktop`.
- [x] Migrate the cursor to `/share/poudland/cursor.bmp`.
- [x] Remove production dependencies on `/test` and the embedded graphical fallback.
- [x] Checkpoint C: prove normal and negative startup paths with bounded guest evidence.

## Phase 5: Automated Acceptance and Handoff

- [x] Separate `/test` mutation profiles from production-root boot profiles.
- [x] Keep the reusable System Image immutable: use a temporary snapshot for the
  production-root profile and copies or a disposable test image elsewhere.
- [x] Run installed-ELF and root failure profiles serially at 16 MiB where applicable.
- [x] Pass `desktop-smoke` through Root Locator, Root Switch, and disk init.
- [x] Pass the separate `desktop-soak-10m` through the same production chain.
- [x] Confirm reusable image hashes are unchanged and failure artifacts are bounded.
- [x] Retain current 16 MiB production-chain evidence: root negative
  `20260810T150712Z`, desktop smoke `20260810T150906Z`, and the independently
  reviewed 600-second/10-heartbeat soak `20260810T152529Z`.
- [x] Update operational docs only after the implementation is proven.
- [x] Write the implementation handoff with exact commands, markers, results, and remaining risks.
- [x] Final checkpoint: review the worktree and confirm no generated artifacts are tracked.

## Explicitly Deferred

- [ ] Implement a true bounded tmpfs and mount it at `/tmp` when needed.
- [ ] Add writable FrogFS volumes for `/home` and `/var` when needed.
- [ ] Design boot `root=`/`init.mode` transport and UUID selection.
- [ ] Add a real TTY Init.
- [ ] Design a combined CompactFlash boot/system/install layout.
