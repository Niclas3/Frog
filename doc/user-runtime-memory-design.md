# User Runtime Memory Design

Status: Accepted for the Poudland P0 runtime

Frog user programs obtain private writable memory through anonymous mappings. Object allocation policy lives in the user runtime rather than in kernel `malloc` and `free` syscalls.

## Goals

- Supply the multi-megabyte backbuffer and small object allocations required by Poudland and `desktop.c`.
- Give `mmap`, `munmap`, `malloc`, and `free` precise failure and lifetime semantics.
- Preserve process isolation across `fork`, release mappings on `exec` and exit, and avoid trusting allocator metadata in the kernel.
- Run the graphical milestone within the first hardware target's 16 MiB of RAM.

## Kernel Mapping Contract

The first anonymous mapping form is:

```c
mmap(NULL, page_aligned_length,
     PROT_READ | PROT_WRITE,
     MAP_PRIVATE | MAP_ANONYMOUS,
     -1, 0);
```

Required behavior:

- `addr` must be `NULL`; fixed-address anonymous mappings remain unsupported.
- Length must be nonzero, page aligned, and no greater than `VM_MAP_MAX_LENGTH`.
- `fd` must be `-1` and `offset` must be zero.
- The kernel reserves a non-overlapping user virtual range, allocates every physical page immediately, clears every byte to zero, and returns the base address.
- Allocation is all-or-nothing. Failure returns the normal negative errno through the raw syscall and `MAP_FAILED` through the user wrapper; no partial VMA remains.
- `munmap` initially requires the exact base address and length of one active mapping. Partial unmap and VMA splitting are deferred.
- Anonymous mappings are private process memory. `fork` gives the child an independent copy with identical initial bytes; writes after the fork do not affect the other process.
- A successful `execv` replaces all earlier anonymous mappings. Process exit releases them.
- Device mappings such as `/dev/fb0` retain their existing explicit `MAP_SHARED` contract and are not silently treated as anonymous RAM.

The initial implementation eagerly allocates and clears pages. Anonymous demand paging and copy-on-write are future optimizations, not part of the P0 contract.

## User Allocator Contract

The user runtime implements `malloc` and `free` on top of anonymous `mmap` and exact `munmap`.

- Returned storage is aligned to at least 16 bytes.
- `malloc(0)` returns `NULL`.
- Allocation failure returns `NULL`.
- `free(NULL)` is a no-op.
- Large allocations use dedicated page-aligned anonymous mappings.
- Small allocations are suballocated from page-backed user-space arenas.
- An entirely unused arena is returned with `munmap`.
- Invalid pointers, double frees, or writes outside an allocation are application errors, but allocator metadata is never interpreted by the kernel as a trusted heap object.
- The P0 public requirement is `malloc` and `free`; `calloc` and `realloc` are deferred until a consumer requires them.

The legacy `SYS_MALLOC` and `SYS_FREE` numbers are not re-enabled for Poudland. They should be removed or retained only as explicitly unsupported compatibility numbers after call sites migrate.

## Poudland Memory Budget

For the first `desktop.c` milestone:

- Poudland may allocate one full-screen damage-aware backbuffer. At 1024 x 768 x 32 bpp this is 3 MiB.
- A server-rendered solid-color window stores geometry, color, ownership, and stacking state, not a private `width * height * 4` pixel buffer.
- The current `b.bmp` cursor source is loaded from FrogFS as agreed; its repository image is about 9.2 KiB and is released after conversion.
- Client Window Surfaces are outside P0 and receive a separate mapping and memory-budget design.

## Validation

Automated validation runs the graphical path with QEMU configured for 16 MiB and covers:

- zeroed anonymous mappings at minimum and maximum accepted boundaries;
- invalid flags, fd, offset, address, length, and user pointers;
- allocation rollback when any page or metadata reservation fails;
- exact `munmap`, double unmap, and teardown on process exit;
- private data after `fork` and complete replacement after `execv`;
- small and large `malloc`, 16-byte alignment, `malloc(0)`, `free(NULL)`, reuse, and arena release;
- repeated compositor and desktop launch/exit without leaked mappings or physical pages;
- the complete `desktop-smoke` scenario under `-m 16M`.

## Deferred Work

- Anonymous demand paging and copy-on-write fork.
- Partial `munmap`, VMA splitting, protection changes, and fixed mappings.
- `calloc`, `realloc`, allocator hardening, and multiple user threads.
- Shared Window Surface mappings.
