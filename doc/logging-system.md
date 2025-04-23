# Logging System Guide for Frog OS

This document defines the structure, usage, and expectations of the logging system within the Frog operating system project.

## 🧠 Purpose of the Logging System

Logging in Frog OS serves multiple purposes:

- Diagnostics during kernel development
- Real-time visibility into internal state changes
- Assertion failures and panic tracing
- Runtime introspection for user-space integration (planned)

## 🧩 Components

### 1. `printk`
- Core kernel printing function
- Current output: VGA text mode buffer
- Future output: serial port (planned)

### 2. Macros
- `DEBUG(...)`: Developer-oriented debug info
- `INFO(...)`: Informational output for runtime events
- `WARN(...)`: Potential issues, recoverable problems
- `PANIC(...)`: Non-recoverable critical faults, halts the system
- `ASSERT(expr)`: Fails if `expr == false`, with file/line/function info

### 3. `debug.h` / `panic.h`
- Centralized macro definitions
- Dependency-free interface for all kernel modules

## 🛠️ Usage Guidelines

- Use `DEBUG` in development or verbose tracing scenarios
- Use `INFO` for important one-time boot/init prints
- Use `WARN` when something is suspicious but not fatal
- Use `PANIC` to halt the system during kernel errors (like null pointer or page fault without handler)
- Use `ASSERT` for defense programming — it should never fail in production

## 📍 Where to Log From

- **Kernel code only** — user-space cannot call `printk` directly
- Any kernel subsystem (e.g., `fs`, `mm`, `sched`, `device`, `vfs`) can use logging macros
- Avoid logging from interrupt context unless you are certain it's safe

## 🚫 What Not to Do

- Don’t use `printk` for user-facing output (implement TTY or IPC for that)
- Don’t call `printk` from user-space — it must go through a syscall if needed
- Don’t use logging in hot paths (like sched_tick) unless you profile and validate

## 🚧 Planned Extensions

- Loglevel filtering: allow disabling DEBUG/INFO selectively
- Serial output stream (COM1/2)
- Ring buffer for log persistence or dump on crash
- Syscall-based logging from user space (for compositors, etc.)

## 🧪 Testing & Verification

- Boot process should emit visible `INFO(...)` or `DEBUG(...)`
- Invalid memory accesses should trigger `PANIC()`
- Assertions should be verifiable via `ASSERT()` fault output

---

This logging system is designed to evolve with the kernel. Update this document as new output backends or log levels are introduced.
