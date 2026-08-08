# Frog Timekeeping Design

Status: Accepted for the Poudland P0 runtime

Frog exposes stable realtime and monotonic semantics without requiring a timestamp counter or realtime-clock hardware. The first i386 implementation uses the 8254-compatible PIT for elapsed time and treats RTC calendar time as an optional boot-time origin.

## Public ABI

Time seconds are signed 64-bit values even though i386 pointers and addresses remain 32-bit:

```c
typedef signed long long int_64;
typedef int_64 time_t;

struct timespec {
        time_t tv_sec;
        int_32 tv_nsec;
};

struct timeval {
        time_t tv_sec;
        int_32 tv_usec;
};
```

The supported calls are:

- `clock_gettime(CLOCK_MONOTONIC, ...)` for elapsed time, deadlines, scheduling, and `wait2`.
- `clock_gettime(CLOCK_REALTIME, ...)` for UTC calendar time when a valid RTC origin exists.
- `gettimeofday(..., NULL)` as the realtime microsecond representation.
- `settimeofday` returns `-ENOSYS` until setting policy and privilege checks are designed.

`tv_nsec` is normalized to 0 through 999,999,999 and `tv_usec` to 0 through 999,999. The ABI unit does not promise hardware nanosecond or microsecond resolution.

The Intel386 SX executes 32-bit instructions and defines signed and unsigned quad-word data even though it has a 16-bit external data bus and a 24-bit physical address bus. The compiler implements the required 64-bit add and compare operations as short i386 instruction sequences over two 32-bit halves. The physical bus width therefore limits RAM capacity and transfer cost, not the range of a C time value.

Reference: [Intel386 SX Microprocessor datasheet](https://www.ardent-tool.com/CPU/docs/Intel/386/datasheets/240187-001.pdf).

## Monotonic Clock

- PIT channel 0 runs in the accepted periodic mode at a real 1000 Hz target rate for P0.
- The divisor high byte is written from `counter_value >> 8`, after the shift rather than after truncation.
- Each IRQ advances normalized monotonic seconds and nanoseconds using constants derived from the actual PIT input frequency and programmed divisor.
- A remainder accumulator preserves the fractional nanoseconds without performing division in the interrupt handler.
- Monotonic time never moves backwards and is independent of realtime availability or correction.
- Public deadline arithmetic uses the normalized time representation. `wait2` continues to accept relative integer milliseconds and rounds expiry outward so it does not return early.
- The 1000 Hz interrupt cost is measured on the 386SX-40 target. A later clock-event frequency change must not change the public time ABI.

The current 32-bit `ticks` value may remain temporarily as timer-wheel indexing state, but it is not the public monotonic clock and wrap-safe comparisons are mandatory. Long-lived time comes from the 64-bit seconds timeline.

## Atomic Snapshot

An i386 cannot atomically load the complete 64-bit seconds field. On the initial single-CPU target, the kernel obtains a snapshot by:

1. disabling local interrupts for a short bounded section;
2. copying seconds, nanoseconds, and realtime-origin state into kernel locals;
3. restoring interrupts;
4. normalizing or converting the local values;
5. copying the completed result to user memory.

The kernel never accesses user memory with interrupts disabled. A future SMP implementation replaces this snapshot mechanism with an appropriate sequence counter or lock without changing the ABI.

## Optional Realtime Clock

RTC presence is not required for Poudland startup. A usable realtime origin requires all of the following:

- RTC valid/status state indicates retained data;
- reads avoid the update-in-progress interval;
- consecutive calendar snapshots stabilize;
- second, minute, hour, day, month, and year values pass range and calendar validation.

Without confirmed century information, Frog interprets RTC years as 2000 through 2099. The hardware clock is treated as UTC. A missing, unwired, unpowered, unstable, or invalid RTC leaves realtime unavailable; `CLOCK_REALTIME` and `gettimeofday` return `-ENODATA`, while `CLOCK_MONOTONIC`, `wait2`, scheduling, and Poudland continue normally.

When RTC data is valid, it is converted once at boot:

```text
realtime = rtc_boot_utc + monotonic_elapsed
```

Normal clock reads do not repeatedly access CMOS ports.

## Poudland Use

Poudland Frame Deadlines use `CLOCK_MONOTONIC`. They never use `gettimeofday`, RTC state, or wall-clock corrections. The initial target presents accumulated damage at no more than approximately 60 frames per second and does not issue catch-up frames after a delay.

## Validation

Automated validation covers:

- the programmed PIT low and high divisor bytes and measured tick count;
- normalized nanosecond carry and fractional-remainder accumulation;
- monotonic non-decrease, deadline expiry, `wait2` rounding, and 32-bit tick wrap boundaries;
- coherent 64-bit snapshots across simulated low-half carry;
- `CLOCK_MONOTONIC` operation with no RTC;
- valid UTC RTC conversion for leap days and the 2000-2099 boundary;
- invalid status, update-in-progress, unstable reads, invalid calendar fields, and `-ENODATA` fallback;
- `gettimeofday` conversion from realtime nanoseconds to microseconds;
- Poudland frame scheduling under the corrected clock.

## Deferred Work

- Runtime realtime setting, privilege checks, clock slewing, and external synchronization.
- Century sources beyond the accepted 2000-2099 RTC interpretation.
- Tickless PIT programming or alternative detected clock sources.
- SMP snapshot synchronization.
