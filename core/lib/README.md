# Frog System Libraries

## lib/ — Core Utilities and Pure Data Structures

This directory provides **side-effect-free**, reusable modules  
that implement common low-level utilities and data structures for Frog OS.  
All code in this directory **must not depend** on:

- Thread scheduling
- File systems
- Hardware I/O or drivers
- Locking or synchronization primitives

## ✅ Pure Modules

| File         | Description                         |
|--------------|-------------------------------------|
| `string.c`   | Memory and string ops (`memcpy`, `strlen`, etc.) |
| `bitmap.c`   | Bit-level manipulation utilities     |
| `fifo.c`     | Lock-free ring buffer implementation |
| `list.c`     | Inline doubly linked list macros     |
| `stdio.c`    | Format output to memory only (no side effects) |
| `hashmap.c`  | simple hashmap, refine it later when checkout compositor |


- All headers for these modules live in `include/frog/`.
- Keep the API clean and state-free.
- Suitable for future user-space reuse or early boot stage usage.

