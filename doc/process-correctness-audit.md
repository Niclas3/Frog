# Process and Thread Correctness Audit

This note describes the process code that is active on `refine/code_arch`, the
invariants used during the audit, the changes made, and the remaining limits.
It is intended as the starting point for the next engineer working in this
area.

## Active feature set

The current build includes `threads.c`, `sched.c`, `semaphore.c`, `process.c`,
`fork.c`, `exit.c`, `switch.s`, the timer/softirq path, and the fork/exit/wait
syscall registrations. It currently provides:

- page-aligned TCB plus kernel stack storage;
- a single-CPU round-robin ready queue and a dedicated idle thread;
- creation of kernel threads and the initial ring-3 process;
- eager address-space copying in `fork()`;
- zombie exit state, parent wakeup, `wait()`, and init adoption;
- binary sleeping semaphores and recursive sleeping locks.

`exec.c` is not built. `execv`, `getpid`, user allocation, and several other
declared process syscalls are not registered and return `-ENOSYS`. TIDs, SMP,
process groups, signals, and copy-on-write are not implemented.

## State and ownership model

The scheduler relies on these invariants:

- `RUNNING` is the CPU's current task and is not on the ready list.
- `READY` is on the ready list, except for the idle task, which is never queued.
- `BLOCKED` and `WAITING` are not on the ready list.
- `HANGING` is a user-process zombie retained on the all-task list until its
  parent calls `wait()`.
- An unpublished task owns its PID, TCB page, virtual-address bitmap, page
  directory, and any copied user pages. A failed creator releases them in the
  reverse order.
- A published child owns retained file references. A current process closes
  those references and releases its address space before becoming a zombie;
  its parent later releases the page directory holder, PID, and TCB.

`thread_publish()` is the single ready/all-list publication point. Address-space
creation and destruction live in `process.c`; `fork.c` copies state but uses the
same destruction path for rollback.

## Correctness fixes

The audit found and addressed the following defects:

- PID exhaustion wrote beyond the PID bitmap. Allocation now fails cleanly and
  every task/process creator rolls back its PID and TCB.
- Thread creation dereferenced failed page allocations and copied names without
  a bound. Inputs and allocations are checked and names are terminated.
- Fresh and forked list nodes used incompatible NULL/self-linked states. All
  unpublished nodes are now initialized as self-linked.
- Ready/all-list publication was open to timer preemption. Publication is now a
  shared IRQ-protected operation.
- The timer's reschedule bit was never cleared and a time slice lasted one tick
  too long. Scheduling consumes the bit and the timer expires on the final tick.
- The idle task could enter the normal ready queue or be switched to itself. It
  is now kept outside the queue and selected only when the current task cannot
  run.
- `schedule_timeout()` queued a timer but left the caller runnable, so it did
  not sleep. It now performs timer insertion, WAITING transition, and schedule
  atomically with IRQs disabled.
- `thread_auth_block()` could mark one task blocked while scheduling another.
  The legacy API now requires the target to be the current task.
- The softirq walker could advance beyond its handler array. It now consumes a
  bounded pending snapshot for a bounded number of rounds.
- Sleeping locks could be entered from IRQ context or released by a non-owner.
  Both cases are rejected by assertions.
- `process_execute()` and `fork()` leaked partial bitmaps, page directories,
  user pages, PIDs, and TCB pages on allocation failure. They now use staged
  reverse-order rollback.
- The initial user stack is allocated before process publication, so stack OOM
  is a creation failure rather than a ring-3 jump with an invalid stack.
- Temporary CR3 switches during fork and address-space destruction could be
  interrupted while `current_thread` still named another address space. Those
  windows are now IRQ-protected.
- A failed fork page mapping could deadlock by acquiring the same memory lock a
  second time. It now unlocks and returns failure; page-table allocation failure
  also unwinds the user frame.
- `exit()` assumed its parent lookup always succeeded, and `wait(NULL)`
  dereferenced NULL. Parent wakeup and optional status storage are now guarded.
- The boot main task released its own TCB/stack before `switch_to()` saved that
  stack. A current task is no longer freed before switching.

## Runtime validation

Run:

```sh
./scripts/qemu-test.sh process-smoke
```

The host runner is `scripts/qemu-test.sh`. The guest protocol is implemented in
`core/kernel/qemu_test.c`, and the process cases are in
`core/kernel/thread/process_regression.c`. The profile proves:

- the parent and child receive their expected fork results;
- a single runnable thread can yield without remaining on the ready queue;
- a child write to its copied stack does not alter the parent's stack;
- the parent receives the child's PID and exit status;
- the zombie is reaped and a later wait reports no child.

Success is quiet and leaves only
`build/qemu-test/process-smoke-result.json`. Failure retains `debugcon.log`, the
QEMU trace, build log, and disposable disks under the timestamped result
directory printed by the runner.

## Remaining limits and recommendations

- There is no general deferred TCB reaper. The early main task can safely switch
  away because its stack page is fixed, but a future terminating kernel thread
  needs another task to free its TCB after the switch. Kernel thread functions
  therefore still must not return.
- `wait()` accepts a raw user pointer. Proper `copy_to_user()` validation is a
  cross-syscall memory-safety task and remains required before treating syscall
  pointers as hostile.
- `pid2thread()` protects lookup but returns a raw pointer after restoring IRQs.
  This is sufficient for the current UP call sites that hold an outer IRQ-off
  section, but it is not an SMP lifetime contract. Prefer a callback performed
  under the task-registry lock when SMP work begins.
- `general_tag` is reused by ready queues and several wait queues. The state
  invariant makes that valid today but tightly couples queue membership. Split
  run-queue and wait-queue nodes before adding richer blocking primitives.
- The new profile exercises timer preemption indirectly but does not yet measure
  time-slice counts or `schedule_timeout()` duration. Add deterministic virtual
  clock hooks before asserting exact tick timing.
- Process address spaces are copied eagerly. Copy-on-write should be considered
  only after page-frame ownership and page-fault recovery have dedicated tests.

## Suggested reading order

1. `core/kernel/thread/process_regression.c` for externally visible contracts.
2. `core/init/init.c` and `core/arch/x86/entry/syscall-init.c` for entry paths.
3. `core/kernel/thread/threads.c` for states, queues, and publication.
4. `core/kernel/thread/process.c` for address-space ownership.
5. `core/kernel/thread/fork.c` and `exit.c` for parent/child lifecycle.
6. `core/kernel/thread/sched.c`, `softirq.c`, and `semaphore.c` for timing and
   synchronization.
