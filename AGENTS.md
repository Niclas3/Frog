# Repository Guidelines

## Project Structure & Module Organization

Frog is a home-brew x86 OS project. The bootloader lives in `booter/`; kernel and user-facing OS code are under `core/`. Important kernel areas include `core/arch/`, `core/block/`, `core/drivers/`, `core/fs/`, `core/include/`, `core/init/`, `core/kernel/`, `core/lib/`, `core/mm/`, and `core/net/`. Filesystem work is split by implementation under `core/fs/`, for example `vfs/`, `frogfs/`, `devfs/`, and `packagefs/`. Repository automation belongs in `scripts/`; docs belong in `doc/`; helper tools live in `tools/`. Disk images such as `hd.img` and `hd80M.img` are runtime assets, not normal source edits.

## Product Goal and Definition of Done

Frog's primary goal is a repeatable QEMU boot into a graphical, Windows-like desktop while retaining a text diagnostic path. The first desktop milestone is complete only when a clean checkout can run one documented command that builds every artifact, boots without manual disk editing, enters ring 3, displays a framebuffer desktop, accepts PS/2 keyboard and mouse input, and keeps at least two focusable, draggable client windows responsive for 10 minutes without a panic, allocator corruption, or disk corruption.

Treat `core/apps/`, `core/arch/x86/platform/pc/lfbvideo.c`, `core/drivers/tty/`, `core/fs/packagefs/`, and `core/init/main_bak.c` as legacy design references, not working subsystems. Do not bulk-enable them or restore the deleted monolithic filesystem. Port one dependency at a time to the current VFS, devfs, fd, process, and driver APIs.

## Milestone Order

1. **Reliable baseline:** fix paging-frame ownership, IRQ save/restore and timer scheduling, partition-relative I/O, syscall dispatch validation, and the build/package path. Gate destructive boot self-tests behind a build option. Add a noninteractive QEMU smoke test while keeping text diagnostics working.
2. **User runtime:** build and package a disk ELF; make `execv`, user allocation, `ioctl`, `getpid`, and basic time syscalls usable; launch the ELF from init.
3. **Framebuffer slice:** select a conservative VBE linear mode, register `/dev/fb0` through the current chardev/devfs APIs, expose validated mode information and mapping, and run a ring-3 color-bar or pixel test.
4. **Input and compositor:** read `/dev/input/event0` and `/dev/input/event1`, implement poll/wakeup, render a cursor and two windows, and verify focus, drag, close, and repaint. Start in one process if useful; add client IPC after rendering and input are stable.
5. **Desktop shell:** split compositor and clients, then add simple window chrome, a panel/launcher, and QEMU regression scenarios for launch, focus, drag, close, persistence, and reboot.

Do not prioritize networking, POSIX breadth, x86-64, advanced filesystem features, alpha/rotation/animation, or visual polish until milestones 1-4 are green.

## Build, Test, and Development Commands

- `./scripts/CI.sh`: clean kernel-only compile smoke test.
- `make -C core core`: build the kernel directly from `core/` for iteration.
- `make -C core debug`: build the symbol-bearing kernel for GDB.

The current release package/run path is not authoritative until it builds its own bootloader, kernel, font, and application prerequisites and consumes the artifacts at their actual output paths. Never rely on stale files under `core/build/` or `core/apps/build/`, and do not use `sudo` for QEMU unless a host device setup explicitly requires it. Keep automation in `scripts/`, document the canonical command in `scripts/README.md`, and make CI exercise that same artifact graph.

## Coding Style & Naming Conventions

This repository is primarily C and assembly. Follow the surrounding C style: K&R-style braces, project typedefs such as `uint_32` and `int_32`, and `snake_case` names for functions and variables. Keep headers close to their module unless the declaration is intentionally shared through `core/include/`. Prefer small, module-local changes over broad refactors, especially around VFS and concrete filesystem boundaries.

## Testing Guidelines

There is no mature unit test framework yet. A compile-only result is not a boot result. After boot, driver, memory, filesystem, process, or UI changes, record the exact QEMU scenario and observed success marker. Prefer automated serial/debug-port markers and bounded smoke tests over visual guesses. Filesystem tests must use disposable image copies and cover partition boundaries as well as create/open/read/write/reopen/unlink behavior.

Follow `doc/qemu-automated-validation.md` when implementing the noninteractive QEMU runner, guest markers, exit protocol, and later QMP-based graphical checks.

Do not inspect or edit `*.img`, `*.bin`, or `*.bmp` payloads unless the task explicitly concerns their generation or packaging. Never mutate the source disk images during routine boot tests; work on disposable copies.

## Commit & Pull Request Guidelines

Recent commits use `<type>(scope): summary`, for example `feat(frogfs): mount block backed frogfs from vfs`, `fix(memory): guard kasan free for large arenas`, and `compile(ci): add kernel build smoke script`. Common types include `feat`, `fix`, `compile`, and `refine`. Pull requests should include the changed subsystem, the commands run, any QEMU/manual test notes, and linked issues when available.
