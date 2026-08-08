---
status: accepted
---

# Use PIT time for intervals and RTC time for the wall clock

Frog's i386 baseline uses the M6117D-compatible 8254/PIT channel 0 and IRQ0 to advance a wrap-safe software monotonic timeline; scheduling, `wait2` timeouts, and Poudland Frame Deadlines use that timeline. A working RTC is optional: when detected with valid retained calendar state, it is read at boot to establish realtime and `gettimeofday`, not to measure intervals; otherwise realtime calls return `-ENODATA` while monotonic consumers continue normally. This works on Frog's first 386SX hardware without assuming an RTC, TSC, local APIC, HPET, or ACPI, while later platforms may add detected clocks without changing the public time semantics.
