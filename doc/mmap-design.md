# Frog `mmap` v1 design

Status: proposed design; no implementation is included in this change.

## 1. Objective

Frog needs a user-space mapping API so that the compositor can write the VBE
linear framebuffer without issuing one syscall for every copy or pixel update.
The first consumer is `/dev/fb0`, but the VM/VFS boundary must not be tied to
one ioctl or one physical address.

The v1 target is deliberately narrow:

- map the complete `/dev/fb0` aperture into one user address space;
- support only shared, readable and writable, device-backed mappings;
- make close, fork, exit and unmap behavior explicit;
- preserve the distinction between allocator-owned RAM and borrowed device
  memory;
- make the result testable from a real ring-3 program under QEMU.

This is not an anonymous-memory allocator. `malloc` and the existing user-page
allocator remain separate until a later VM consolidation.

## 2. Current constraints

The current code cannot safely implement `mmap` by exposing `put_page()` or by
reviving `IO_VID_ADDR`:

- a process has a page directory and a virtual-address bitmap, but no VMA list;
- `put_page()` overwrites a PTE and always installs `USER | WRITE | PRESENT`;
- process teardown treats every present user PTE as allocator-owned RAM and
  calls `free_phy_page()` on it;
- fork copies every page represented by the user bitmap into newly allocated
  RAM, which is incorrect for MMIO;
- the page-fault handler currently treats almost every address below 3 GiB as a
  valid lazy-RAM allocation;
- the syscall entry passes at most four register arguments;
- VFS has an incomplete `file_operations.mmap` placeholder but no generic
  `vfs_mmap()` path or mapping-owned file reference;
- the kernel PDEs and recursive page-table PDE are currently user-accessible;
- the current ring-3 `init` path executes a kernel-linked high-address function,
  so tightening those PDE permissions first would break process startup.

These are design constraints, not reasons to let a driver write arbitrary page
tables. The VM core must own address selection, PTE installation and teardown.

## 3. v1 user API

The source-level API follows the familiar shape:

```c
void *mmap(void *addr, uint_32 length, uint_32 prot,
           uint_32 flags, int_32 fd, uint_32 offset);
int_32 munmap(void *addr, uint_32 length);
```

The initial public constants are:

```c
#define PROT_NONE       0x00
#define PROT_READ       0x01
#define PROT_WRITE      0x02
#define PROT_EXEC       0x04

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_FAILED      ((void *)-1)
```

Only this exact v1 request is accepted for `/dev/fb0`:

```c
mmap(NULL, info.map_length,
     PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0)
```

The restrictions are:

- `addr` must be `NULL`; address hints and `MAP_FIXED` are unsupported;
- `length` must equal the driver's page-aligned `map_length`;
- `offset` must be zero;
- `prot` must be exactly `PROT_READ | PROT_WRITE`;
- `flags` must be exactly `MAP_SHARED`;
- `fd` must refer to an `O_RDWR` file whose operations support `mmap`;
- the mapping is installed eagerly;
- `munmap` accepts only the exact start and complete original length of one VMA.

Partial unmap is intentionally rejected with `-EINVAL`. This is a documented
non-POSIX limitation of v1, not behavior to preserve as a permanent ABI rule.

## 4. Syscall ABI

The x86 syscall entry currently carries four arguments in `ebx`, `ecx`, `edx`
and `esi`. It should not be expanded only for this call. The user wrapper passes
one pointer to a fixed-layout argument block:

```c
struct frog_mmap_args {
    uint_32 addr;
    uint_32 length;
    uint_32 prot;
    uint_32 flags;
    int_32  fd;
    uint_32 offset;
};
```

`SYS_MMAP` is therefore a one-argument syscall. The kernel validates the full
user range and copies the structure once with `copy_from_user()` before using
it. It must never keep or repeatedly dereference the user pointer.

`SYS_MUNMAP` takes two register arguments, `addr` and `length`.

Syscall numbers are ABI. The current enum uses implicit increments, so mmap work
must first preserve the numeric values of all existing calls, preferably by
making them explicit. `SYS_MMAP`, `SYS_MUNMAP` and any test-only synchronization
call are appended after the last existing number and before `SYS_NR_COUNT`; they
must never be inserted into the middle of the enum.

The raw syscall convention is:

- success from `SYS_MMAP`: the chosen user virtual address;
- success from `SYS_MUNMAP`: zero;
- failure: a negative Frog errno value.

The C wrapper translates a negative result to `MAP_FAILED` and records the
positive errno when the user runtime has errno support. A raw wrapper can remain
available to early applications.

To make signed 32-bit result testing unambiguous, v1 mappings are allocated only
inside this half-open window:

```text
0x40000000 <= mapping < 0x80000000
```

The complete aligned mapping must fit in the window. This also keeps it away
from the current user stack near `0xc0000000`. The existing process bitmap is
still consulted so that a mapping cannot overlap legacy user allocations.

## 5. Required VM data model

The process address-space fields should move behind an `mm_struct` owned by the
process:

```c
struct mm_struct {
    uint_32 *pgdir;
    virtual_addr user_vaddr;
    struct list_head vma_list;
    uint_64 generation;
    struct lock mmap_lock;
    spinlock_t pt_lock;
};
```

The VMA list is ordered by start address and contains non-overlapping ranges:

```c
enum vm_area_state {
    VM_PREPARING,
    VM_ACTIVE,
    VM_UNMAPPING,
};

enum vm_backing_type {
    VM_BACKING_RAM_OWNED,
    VM_BACKING_DEVICE_BORROWED,
};

struct mmio_resource {
    uint_64 start;
    uint_64 end;               /* exclusive */
    enum vm_cache_mode cache_mode;
    struct device *owner;
    refcount_t refs;
};

struct vm_mapping {
    refcount_t refs;           /* one reference per VMA */
    enum vm_backing_type backing_type;
    struct file *file;         /* independent object reference */
    struct device *device;
    struct mmio_resource *resource;
    struct vm_operations *vm_ops;
    void *private_data;
};

struct vm_area {
    struct list_head elem;
    struct mm_struct *mm;
    uint_32 start;             /* inclusive, page aligned */
    uint_32 end;               /* exclusive, page aligned */
    uint_32 prot;
    uint_32 flags;
    uint_32 page_offset;
    enum vm_area_state state;
    struct vm_mapping *mapping;
};
```

The exact field split may follow existing naming conventions, but these facts
must be represented explicitly:

- virtual range and permissions;
- backing ownership and the trusted physical resource;
- the file/device lifetime reference;
- device cleanup operations;
- the identity shared by VMAs created through `fork`.

An independently created `mmap` owns one `vm_mapping`. A forked VMA increments
that same object's reference count rather than invoking the driver's mmap
callback again. The last VMA drops the last mapping reference and performs the
single device unpin/close operation. This makes the distinction between a new
mapping and an inherited mapping explicit.

`vm_mapping.refs` is an atomic refcount independent of every `mm_struct` lock.
`vm_mapping_get_live()` uses get-unless-zero and rejects overflow;
`vm_mapping_put()` uses decrement-and-test. Exactly the caller that transitions
the count to zero runs the non-failing destructor after all VM locks are gone,
and a zero-count object can never be revived. This remains required on one CPU
because a sleepable path may be interleaved by the scheduler.

`mm_struct.generation` increments on every VMA, bitmap or user-PTE mutation. It
allows operations such as fork to size resources outside the lock and then
verify that the parent snapshot did not change before committing.

During the transition, the virtual bitmap means "this user virtual page is
reserved", while `PTE.P` means "this page is currently resident". The VMA list
is authoritative for all `mmap`-created ranges. Existing eagerly allocated
heap/stack pages can remain bitmap-and-PTE managed until they are converted to
VMAs.

Every PTE created by `mmap` must belong to exactly one VMA. A device VMA never
causes its PFNs to enter a physical RAM pool bitmap.

VMA state preserves that ownership invariant during a transaction. A VMA is
inserted as `VM_PREPARING` before its first PTE is installed, becomes
`VM_ACTIVE` only after all PTEs are ready, and changes to `VM_UNMAPPING` before
the first PTE is cleared. It is removed only after the last PTE is gone. Fault,
fork and exit paths may use only `VM_ACTIVE` mappings.

## 6. Page-table primitives

`put_page()` is not an acceptable `mmap` primitive. The VM layer needs checked,
transactional operations with explicit flags and ownership, for example:

```c
uint_32 vm_find_unmapped_area(struct mm_struct *mm, uint_32 length);
int vm_reserve_range(struct mm_struct *mm, uint_32 start, uint_32 length);
int vm_map_pfn_range(struct vm_area *vma, uint_32 physical,
                     uint_32 pte_flags);
int vm_unmap_exact(struct mm_struct *mm, uint_32 start, uint_32 length);
```

The implementation contract is:

- calculate `end`, aligned lengths, PFN counts and physical ends with checked
  64-bit arithmetic before narrowing to 32 bits;
- reject any occupied PTE instead of overwriting it;
- preserve the requested PTE flags;
- allocate the VMA, mapping object and a worst-case number of unpublished
  page-table pages before acquiring `mmap_lock`; reserve enough for every PDE
  that any page-aligned range of the requested length could span;
- after choosing the address, consume only the preallocated page-table pages
  needed for missing PDEs and return unused pages after releasing VM locks;
- on failure, remove only PTEs and page-table pages installed by this attempt;
- invalidate affected TLB entries after removing or changing a present PTE;
- never pass device PFNs to `free_phy_page()`;
- release the reserved virtual bitmap range on rollback and unmap.

Unmap and rollback detach empty page-table pages while holding the VM locks but
only collect their physical frames on a temporary release list. They return
those frames to the physical pool after restoring interrupts and releasing VM
locks. No page-pool allocation or free is allowed while `pt_lock` is held.

Local `invlpg` is sufficient only because mmap v1 is restricted to the current
single-CPU configuration. An SMP build must reject or disable this feature until
it provides cross-CPU TLB shootdown for every CPU that can run the `mm_struct`.

An optional software PTE bit may mark a borrowed-device page as a defensive
assertion, but the VMA is the authoritative ownership record.

## 7. Framebuffer mapping contract

The modern framebuffer driver registers `/dev/fb0` through the existing
character-device and devfs path. The old `lfbvideo.c` ioctl mapping is reference
material only and must not be restored.

Framebuffer metadata exposed to user space contains geometry, not a physical
address:

```c
struct frog_fb_info {
    uint_32 width;
    uint_32 height;
    uint_32 pitch;
    uint_32 bits_per_pixel;
    uint_32 red_position;
    uint_32 red_size;
    uint_32 green_position;
    uint_32 green_size;
    uint_32 blue_position;
    uint_32 blue_size;
    uint_32 visible_length;
    uint_32 map_length;
};
```

`visible_length` is the checked `pitch * height`. `map_length` is the complete
page-aligned range accepted by `mmap`.

Geometry is not proof of physical ownership. Before registering `/dev/fb0`, the
platform must create a trusted `mmio_resource` for the complete framebuffer
aperture. Its start and length come from a platform-owned source such as the PCI
BAR or VBE controller `TotalMemory` information passed by the loader, not from a
user request. Resource registration uses checked 64-bit ranges and rejects an
overlap with E820 allocator-owned RAM or another registered MMIO resource. The
kernel framebuffer alias and every user mapping reference this same resource
and cache policy.

The framebuffer base must be page-aligned, and
`[base, base + map_length)` must fit completely inside that aperture. Padding
past `visible_length` therefore remains device-owned. If the platform cannot
establish an authoritative aperture length, direct LFB mmap is unavailable;
the safe fallback is a page-aligned RAM shadow buffer followed by a checked
kernel blit.

The physical base remains kernel-private. It comes only from this registered
resource, never from an ioctl or `mmap` argument.

The VFS operation becomes:

```c
int_32 (*mmap)(struct file *file, struct vm_area *vma);
```

This is an incompatible change to `struct file_operations`. The declaration,
generic dispatch and every initializer/implementation must move in one
buildable commit. FrogFS should omit the callback or adopt the new signature
and return `-EOPNOTSUPP`; its old four-argument placeholder must not remain.

The generic VM/VFS path owns address selection and page-table edits. The driver
callback, invoked outside `mmap_lock`, may only:

- validate access mode, length, offset, protection and mapping flags;
- pin its device resource and reserve an active mapping;
- fill the generic core's preallocated `vm_mapping` and mark it prepared.

Before that callback, the generic core initializes the provisional VMA with
`start = 0`, `end = normalized_length`, and the checked protection, flags and
page offset. The driver treats those fields as read-only and validates
`end - start`; it never selects an address. After successful preparation, the
generic core relocates the still-unpublished VMA to
`[chosen, chosen + normalized_length)`.

The callback has a strict ownership rule. On error it leaves no reference,
reservation or partially initialized driver state behind; the generic core
drops the still-unprepared object and its file reference. On success the
generic core initializes the unpublished object's refcount to one for its first
VMA; the prepared object consumes that file reference and remains owned by the
VM core. Any later generic failure calls `vm_mapping_put()` exactly once; its
final destructor performs a non-failing device abort/close while the file and
device references are still valid, then drops those references.

The current fd layer must be refactored rather than merely incrementing
`f_count`. It needs separate descriptor/global-slot bookkeeping and file-object
lifetime references:

- each file object has an atomic `refcount_t f_refs`, independent of descriptor
  table and global-slot indices;
- `fdget()` performs fd lookup and `file_get_live()` atomically under the
  fd-table lock, then returns a strong `struct file *` reference;
- closing the last descriptor immediately removes that descriptor/global-table
  lookup entry;
- a VMA/mapping reference can keep the heap-allocated file object alive without
  occupying or requiring a local fd slot;
- `file_get_live()` uses get-unless-zero and rejects overflow;
  `file_put(struct file *)` uses decrement-and-test and lets exactly the
  zero-transition caller perform backend close, inode release and object free.

Closing the original fd therefore does not revoke the mapping, while later use
of that fd number still returns `-EBADF` unless it has been reused normally.

For v1, each `mm_struct` permits one independently created writable `/dev/fb0`
mapping. A mapping cloned by `fork` is represented by the same refcounted
`vm_mapping` and is allowed. A duplicate independent map in the same address
space returns `-EBUSY`; a different process may create its own shared mapping.
The driver counts live `vm_mapping` objects, not VMAs; fork therefore increments
the object's VMA reference count without creating a second device reservation.
Mode changes and device unregister return `-EBUSY` while any mapping object
exists.

Device lifetime uses `LIVE`, `DYING` and `DEAD` states protected by the device
lock. `open` and mmap preparation use `device_get_live()` so they can acquire a
reference only in `LIVE`. Synchronous unregister first blocks new references in
the same lock; if an open file, mapping or other device reference exists, it
restores `LIVE` and returns `-EBUSY`. Only a zero-reference device can have its
devfs node, operations and MMIO resource removed before becoming `DEAD`.

`mmio_resource.refs` is also an atomic refcount. A mapping can acquire it only
through `mmio_resource_get_live()` while holding the owner device lock and
observing `LIVE`; put uses decrement-and-test outside VM locks. The device keeps
one registration reference. Unregister, under the same device lock, may remove
the resource only when no external resource reference exists, then drops that
last registration reference exactly once. A zero-count resource cannot be
revived.

## 8. Cache and protection rules

Framebuffer pages are MMIO, not normal write-back RAM. v1 maps both kernel and
user aliases uncached with a consistent x86 cache type. The x86 page flag API
therefore needs explicit `PWT` and `PCD` definitions; write combining through
PAT is a later optimization.

The user PTE is `PRESENT | USER | WRITE | PWT | PCD`. `PROT_EXEC` is rejected.
On the current 32-bit target there is no NX support, so the kernel cannot claim
to enforce a general non-executable mapping policy. Restricting v1 to the exact
framebuffer protection combination avoids pretending otherwise.

Before this API is considered secure, kernel PDEs and the recursive page-table
PDE must be supervisor-only. User PDEs below 3 GiB remain user-accessible. This
permission change has a prerequisite: ring-3 startup must execute a real user
image or trampoline below 3 GiB, rather than the current kernel-linked
high-address `init` function.

Required ordering for that prerequisite is:

1. add a real low-address user entry path and user-copy helpers;
2. verify ring-3 startup and syscalls through that path;
3. clear `USER` from kernel and recursive mappings in both loader and per-process
   page-directory construction;
4. only then expose `mmap` to applications.

## 9. `mmap` transaction

The generic `mmap` path performs these steps:

1. Validate and copy the user argument block.
2. Check exact v1 flags, protections, length, offset and arithmetic.
3. Use `fdget()` to verify `O_RDWR` and acquire an independent file-object
   reference.
4. Allocate a provisional VMA, its unprepared `vm_mapping`, and a worst-case set
   of page-table pages outside all VM locks.
5. Ask the driver to validate the request and fill/pin that mapping object; the
   prepared mapping consumes the strong file reference on success.
6. Acquire `mm->mmap_lock`, find and recheck a hole in the mmap window, verify
   VMA/PTE/bitmap non-overlap, and reserve the virtual bitmap range.
7. Insert the VMA in address order as `VM_PREPARING` before installing any PTE.
8. Consume preallocated page-table pages and install the device PTEs under
   short `pt_lock` sections, without entering the physical page pool.
9. Switch the VMA to `VM_ACTIVE` under `pt_lock`, release `mmap_lock`, and return
   unused preallocated pages outside both locks.
10. Return the selected address.

Every failure unwinds in reverse order. Any failed call must
leave the process bitmap, page tables, file counts, framebuffer mapping count
and physical RAM allocator identical to their entry state.

If failure occurs after the `VM_PREPARING` VMA is inserted, rollback keeps that
VMA present while clearing every installed PTE, then removes it and releases the
bitmap reservation. Collected page-table frames and the prepared mapping object
are released only after VM locks are dropped. At no point is a present mmap PTE
left without an ownership record.

No driver callback may run while an IRQ-disabled page-table critical section or
physical-pool lock is held.

## 10. `munmap` transaction

v1 `munmap` requires a page-aligned VMA start and its exact original length:

1. Validate range arithmetic and acquire `mmap_lock`.
2. Locate one `VM_ACTIVE` VMA whose `[start, end)` exactly matches the request.
3. Change it to `VM_UNMAPPING` under `pt_lock`, keeping it in the list as the
   ownership record.
4. Clear its PTEs under short `pt_lock` sections, invalidate local TLB entries,
   and collect newly empty page-table frames without freeing them.
5. Remove the now-PTE-free VMA and release the virtual bitmap range.
6. Release `mmap_lock` and restore interrupts.
7. Return collected page-table frames to the pool, then drop the mapping
   reference. The last reference performs the non-failing device close and
   file/device puts.

Step 7 may return page-table RAM allocated by the VM to the pool, but step 4
must never free the framebuffer PFNs themselves.

An access after successful unmap is an invalid user fault. It must terminate the
faulting process, not lazily allocate an ordinary RAM page at the same address.

## 11. Lifecycle semantics

### Close

`close(fd)` drops only the descriptor's reference. The VMA reference keeps the
file and device alive, so framebuffer writes through an existing mapping remain
valid. The last `munmap` or process teardown drops the mapping reference.

### Fork

Fork creates a new `mm_struct` and preserves virtual addresses:

1. Acquire the parent `mmap_lock`, record `generation`, and count the exact VMA
   nodes, bitmap storage, page-directory/page-table pages and allocator-owned
   RAM pages needed by an unpublished child. Do not retain unreferenced parent
   pointers after unlocking.
2. Release the parent lock and preallocate that worst-case child storage and all
   physical frames. No parent or child VM lock is held during allocation.
3. Reacquire the parent lock and compare `generation`. If it changed, release
   the preallocation outside the lock and retry from step 1.
4. While the validated parent snapshot is locked, build the unpublished child:
   clone VMA metadata, use `vm_mapping_get_live()` for each shared mapping, map
   the same framebuffer PFNs with identical protection/cache flags, and copy
   legacy allocator-owned pages into the preallocated RAM frames while skipping
   all VMA ranges. No allocation occurs in this phase.
5. Finish the child only after its complete `mm_struct`, PTEs, bitmap and
   references are consistent. Release the parent lock, return unused
   preallocation outside VM locks, and only then publish the child to the
   scheduler/process lists.
6. On failure, keep the child unpublished, remove only child PTEs, collect its
   pages and acquired mapping references, release the parent lock, then perform
   all `vm_mapping_put()` and physical frees. The parent is unchanged.

Framebuffer contents are never copied. mmap v1 also relies on the current
one-runnable-thread-per-`mm_struct` process model while copying legacy RAM;
shared-mm threads require thread quiescing or copy-on-write before fork can
claim a coherent snapshot.

Parent and child observe the same `MAP_SHARED` framebuffer storage. The driver's
live-object count is unchanged by fork, while the shared `vm_mapping` reference
count increases for the inherited VMA.

### Exit and exec

Address-space teardown first unmaps every VMA while the `mm_struct` and its page
tables are alive. It then frees remaining allocator-owned user RAM, page tables
and the virtual bitmap. Descriptor-table close and VMA close use independent
file references, so either ordering cannot produce a dangling `struct file`.

This requires restructuring the current exit path. Complete VM teardown runs in
a sleepable context with interrupts enabled: acquire `mmap_lock`, mark each VMA
`VM_UNMAPPING` but keep it in the list, clear all of its PTEs, and only then
remove the VMA and collect its mapping/page-table references. Release VM locks
before performing mapping, file, device and physical-pool puts. Only after
those operations and descriptor close are complete may exit enter the short
IRQ-disabled process-list, parent notification, zombie and final scheduling
critical section. `process_release_address_space()` must no longer be called as
a potentially blocking operation from the existing IRQ-disabled region.

Exec, when present, destroys old VMAs using the same teardown path before
installing the new address space.

### Page fault

The page-fault handler must consult explicit VM metadata and serialize with
mmap, munmap, fork and exit. A user-mode fault enters a sleepable VM-fault path
with interrupts enabled, acquires `mmap_lock`, rechecks the VMA and PTE, and uses
`pt_lock` only for the short PTE/state operation. A kernel or atomic-context
fault must not attempt to sleep on `mmap_lock`.

The locked fault path only classifies the result and, for a valid explicit lazy
RAM region, commits a prepared page. For an invalid user fault it records a
`FAULT_KILL_USER` result, releases `pt_lock` and `mmap_lock`, and only then enters
the process-fault termination path. Termination must start with no VM lock held
so exit can reacquire `mmap_lock` for teardown; it must not recursively invoke
exit on the exception stack while still locked.

Since framebuffer v1 is eager, a not-present fault in a `VM_ACTIVE` device VMA
is an invalid mapping, not a request for RAM. `VM_PREPARING` and
`VM_UNMAPPING` are never resolved as RAM. A user fault outside a valid VMA or an
explicitly defined legacy allocation region terminates that process. Kernel
faults remain fatal.

Once classified as `FAULT_KILL_USER`, the same fault is never reconsidered by
the legacy lazy allocator. This prevents a post-unmap access from turning into
RAM during termination.

The current rule "any address below `0xc0000000` may receive a RAM page" must be
removed before `munmap` can have correct semantics.

All other writers of process address-space state, including legacy user-page
allocation/free, stack setup, the executable loader and fork, must use the same
`mmap_lock` for VMA/bitmap reservation and `pt_lock` for PTE publication. It is
not sufficient for only the new mmap code to take these locks.

## 12. Locking

`mmap_lock` protects the VMA list and user virtual bitmap as one consistency
domain and serializes mmap, munmap, fork, exit and user-fault lookup.
`pt_lock` is IRQ-safe and protects short PTE/PDE updates plus VMA state
transitions. The only VM lock nesting is:

```text
mm->mmap_lock -> mm->pt_lock
```

Physical-pool locks are never nested inside either VM lock. Mmap preallocates
the maximum page-table pages before taking `mmap_lock`; unmap and rollback only
detach pages to a release list and free them after both locks are gone. The same
reserve-first, allocate/free-outside, publish-later discipline must be used when
legacy allocation paths are brought under these locks.

The fd-table lock is released after `fdget()` has produced a strong file
reference. Driver prepare/abort and device locks run before or after the VM
critical section, never inside it. The pinned `vm_mapping` makes the resource
stable across that gap.

No allocation, driver callback, file/device put or physical-pool free occurs in
an IRQ-disabled `pt_lock` section. Teardown keeps a `VM_UNMAPPING` ownership
record visible until PTE removal is complete, then performs blocking release
work after the VM state is no longer visible.

The metadata locks avoid silent lock inversion, but v1 runtime support is still
explicitly single-CPU because it has no remote TLB shootdown. SMP support is a
separate prerequisite, not an implied property of this design.

## 13. Error contract

The expected failures are:

| Error | Condition |
| --- | --- |
| `-EFAULT` | mmap argument block is not a completely readable user range |
| `-EBADF` | fd is invalid or already closed |
| `-ENODEV` | file/device has no valid mapping resource |
| `-EACCES` | file was not opened for both read and write |
| `-EINVAL` | bad address, length, protection, flags, offset, or non-exact unmap |
| `-EOPNOTSUPP` | anonymous, private, regular-file, fixed or other unsupported map |
| `-EBUSY` | this mm already maps fb0, or mode/unregister is busy |
| `-ENOMEM` | no VMA metadata, page-table page, or fitting virtual hole |
| `-EOVERFLOW` | length/address/physical-range calculation overflows |
| `-ENOTTY` | unknown framebuffer ioctl |

No error path may leave a partial mapping visible.

## 14. Validation strategy

### VM-level tests

Host-callable or kernel tests should cover:

- mmap-window boundary and overflow calculations;
- first-fit selection with bitmap and VMA holes;
- rejection of any existing PTE or overlapping VMA;
- failure injection after every allocation and PTE installation step;
- complete rollback of bitmap, page tables and reference counts;
- unmap of device PFNs without changing the physical RAM bitmap;
- exact-unmap rejection for prefix, suffix and middle ranges;
- fork cloning of PFNs rather than framebuffer bytes;
- exit after fd close and fd close after unmap.

### QEMU `framebuffer-mmap-smoke`

Keep the existing kernel `framebuffer-smoke` as a lower-level VBE mapping test.
Add a distinct end-to-end test with this chain:

1. Build a separate `CONFIG_FROG_TEST_FRAMEBUFFER_MMAP` profile and boot QEMU in
   the fixed `1024x768x32`, single-CPU mode. It must not enable or call the
   kernel framebuffer painter, which currently stops normal boot.
2. Register and mount devfs through the normal boot path.
3. Launch real low-address ring-3 test code.
4. Open `/dev/fb0` with `O_RDWR` and obtain `frog_fb_info`.
5. Verify representative invalid mmap requests, ioctl user pointers and syscall
   argument blocks return the expected errno.
6. Map the complete framebuffer and draw deterministic RGB bands from ring 3.
7. Require `close(fd) == 0`, verify another operation on the old descriptor
   returns `-EBADF`, then draw a white center rectangle through the still-live
   VMA.
8. Fork; let the child alter a small known rectangle and verify the parent sees
   the shared result. Exercise both parent-first and child-first exit cases.
9. Exercise exact unmap/exit cleanup, a child-only invalid post-unmap access,
   and verify mapping/device reference counts return to zero. Unmapping does not
   clear the pixels already stored in the framebuffer.
10. Only if every guest assertion passed, invoke a new test-only synchronization
    syscall that emits exactly `SYNC framebuffer-mmap-ready` and then waits; it
    must not call the existing test syscall that exits QEMU.
11. After the ready marker, let the host capture the display, compare the full
    frame pixel for pixel with the expected image, and then issue QMP `quit`.

The kernel must not paint the expected test frame before ring-3 execution; that
would allow a broken mmap path to pass. The new test syscall number is always
reserved at its stable appended ABI position; its handler is enabled only for
this profile and returns `-EOPNOTSUPP` otherwise. In the test profile it
validates the zero mapping/reference state, emits the marker, and leaves the VM
running long enough for QMP screenshot capture.

Any guest assertion failure must suppress the ready marker. The host accepts a
screenshot only after observing the matching
`BEGIN profile=framebuffer-mmap-smoke` and exact ready marker, and it rejects any
earlier `CASE ... FAIL`, `MILESTONE ... FAIL`, `ABORT`, panic or unexpected QEMU
exit. The host reports a quiet success. On failure it preserves the debug log,
QMP transcript and screenshot so that only actionable diagnostics are printed.

Additional negative runtime cases should prove that an access after `munmap`
kills only the child process, mode change/unregister returns `-EBUSY` while a
mapping exists, and injected allocation failures leave the OS able to run a
second successful test.

## 15. Implementation sequence

The dependency order is:

1. Trusted RAM/MMIO resource registration, including an authoritative
   framebuffer aperture length and allocator overlap checks.
2. Real user code below 3 GiB, `copy_from_user()`/`copy_to_user()`, and
   supervisor-only kernel/recursive page mappings.
3. `mm_struct`, ordered stateful VMAs, refcounted `vm_mapping`, mmap-window
   reservation and exact unmap.
4. Explicit page-table flags, MMIO cache flags, transactional map/unmap and
   backing-ownership checks.
5. Fork, exit and page-fault integration plus the fd/file-object lifetime
   refactor.
6. Stable appended `SYS_MMAP`/`SYS_MUNMAP` numbers, generic VFS dispatch and
   user wrappers. Migrate every `file_operations.mmap` implementation in the
   same buildable interface-change commit.
7. Modern `/dev/fb0` info operations, device-state handling and mmap resource
   callback.
8. VM fault-injection tests and the separate QEMU
   `framebuffer-mmap-smoke` profile/ready syscall.

Each implementation commit must compile and pass the runtime checks required by
the project commit policy. A commit must not temporarily make all kernel PDEs
supervisor-only while ring 3 still executes the high-address `init` function.

## 16. Non-goals for v1

The following are explicitly deferred:

- anonymous mappings and using `mmap` as `malloc`;
- regular-file mappings and page cache integration;
- `MAP_PRIVATE`, copy-on-write and demand paging;
- `MAP_FIXED`, address hints and mapping replacement;
- partial `munmap`, VMA split/merge and `mprotect`;
- nonzero device offsets and subresource mappings;
- executable mappings and a general W^X policy;
- write-combining/PAT optimization;
- SMP execution before remote TLB shootdown exists;
- coherent fork with multiple runnable threads sharing one `mm_struct`;
- `msync`, hot-unplug and framebuffer mode changes with live mappings.

## 17. Definition of done

The mmap v1 work is complete only when all of these are true:

- a real ring-3 program maps `/dev/fb0` through the public API and produces the
  host-verified frame;
- the mapped physical range is wholly contained in a trusted, non-RAM
  framebuffer resource with one consistent cache policy;
- user code cannot read/write kernel or recursive page-table mappings;
- no user argument can select a physical address or replace an existing PTE;
- close, fork, exact unmap and exit follow the documented reference semantics;
- device teardown never frees MMIO through the RAM allocator;
- invalid post-unmap access cannot be converted into anonymous RAM;
- every failure point rolls back without leaked VM, file or device state;
- the mmap test cannot emit its ready marker after any guest failure and all
  cleanup checks complete before QMP capture;
- the lower-level kernel framebuffer smoke and the end-to-end mmap smoke both
  pass independently.
